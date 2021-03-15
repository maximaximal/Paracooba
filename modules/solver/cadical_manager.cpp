#include <algorithm>
#include <atomic>
#include <chrono>
#include <list>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "cube_source.hpp"
#include "paracooba/common/message.h"
#include "paracooba/common/noncopy_ostream.hpp"
#include "solver_assignment.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"

#include <paracooba/broker/broker.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/log.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>
#include <paracooba/solver/cube_iterator.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>

#include <cereal/archives/binary.hpp>

namespace parac::solver {
struct CaDiCaLManager::Internal {
  static const size_t CNFStatisticsNodeWindowSize = 10;
  Internal(size_t availableWorkers)
    : acc_averageSolvingTime(
        boost::accumulators::tag::rolling_window::window_size =
          CNFStatisticsNodeWindowSize * availableWorkers) {}

  template<typename T>
  using Allocator =
    boost::fast_pool_allocator<T,
                               boost::default_user_allocator_new_delete,
                               boost::details::pool::default_mutex,
                               8>;
  struct SolverTaskWrapper;
  using SolverTaskWrapperList =
    std::list<SolverTaskWrapper, Allocator<SolverTaskWrapper>>;
  struct SolverTaskWrapper {
    SolverTaskWrapperList::iterator it;
    SolverTask t;
  };

  static SolverTaskWrapperList solverTasks;
  static std::mutex solverTasksMutex;

  std::vector<CaDiCaLHandlePtr> solvers;
  boost::container::vector<bool>
    borrowed;// Default constructor of bool is false. Must be a boost vector, as
             // std vector is a bitset, which breaks multithreading.

  std::mutex averageSolvingTimeMutex;
  ::boost::accumulators::accumulator_set<
    double,
    ::boost::accumulators::stats<::boost::accumulators::tag::rolling_mean>>
    acc_averageSolvingTime;

  std::atomic_size_t waitingSolverTasks = 0;

  std::mutex satAssignmentMutex;
  std::unique_ptr<NoncopyOStringstream> satAssignmentOstream;
};

CaDiCaLManager::Internal::SolverTaskWrapperList
  CaDiCaLManager::Internal::solverTasks;
std::mutex CaDiCaLManager::Internal::solverTasksMutex;

static size_t
getAvailableWorkersFromMod(parac_module& mod) {
  parac_handle* handle = mod.handle;
  assert(handle);
  auto modRunner = handle->modules[PARAC_MOD_RUNNER];
  if(!modRunner) {
    return 0;
  }
  auto runner = modRunner->runner;
  if(!runner) {
    return 0;
  }
  assert(runner);
  return runner->available_worker_count;
}

CaDiCaLManager::CaDiCaLManager(parac_module& mod,
                               CaDiCaLHandlePtr parsedFormula,
                               SolverConfig& solverConfig)
  : m_internal(std::make_unique<Internal>(getAvailableWorkersFromMod(mod)))
  , m_mod(mod)
  , m_parsedFormula(std::move(parsedFormula))
  , m_solverConfig(solverConfig) {

  uint32_t workers = 0;

  if(mod.handle && mod.handle->modules[PARAC_MOD_RUNNER]) {
    workers =
      mod.handle->modules[PARAC_MOD_RUNNER]->runner->available_worker_count;
  }

  if(workers > 0) {
    assert(m_parsedFormula);
    parac_log(
      PARAC_SOLVER,
      PARAC_TRACE,
      "Generate CaDiCaLManager for formula in file \"{}\" from compute node {} "
      "for {} "
      "workers. Copy operation is deferred to when a solver is requested.",
      m_parsedFormula->path(),
      m_parsedFormula->originatorId(),
      workers);

    m_internal->solvers.resize(workers);
    m_internal->borrowed.resize(workers);

    // Generate cubes if required.
    if(solverConfig.CaDiCaLCubes()) {
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Because --cadical-cubes is active, pregenerate cubes as part "
                "of formula parsing step of {} from {}.",
                m_parsedFormula->path(),
                m_parsedFormula->originatorId());
      m_parsedFormula->lookahead(solverConfig.InitialCubeDepth(),
                                 solverConfig.InitialMinimalCubeDepth());
    }
  } else {
    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "Generate dummy CaDiCaLManager for formula that was not parsed "
              "locally, as there are 0 workers.");
  }
}
CaDiCaLManager::~CaDiCaLManager() {
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Destroy CaDiCaLManager for formula in file \"{}\" from {}.",
            m_parsedFormula->path(),
            m_parsedFormula->originatorId());
}

CaDiCaLManager::CaDiCaLHandlePtr
CaDiCaLManager::takeHandleForWorker(parac_worker worker) {
  auto& s = m_internal->solvers;
  auto& b = m_internal->borrowed;

  assert(worker < s.size());
  assert(!b[worker]);

  b[worker] = true;

  if(!s[worker]) {
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Creating copy of root formula in file \"{}\" from {} because a "
              "solver object was requested from CaDiCaLManager.",
              m_parsedFormula->path(),
              m_parsedFormula->originatorId());
    s[worker] = std::make_unique<CaDiCaLHandle>(*m_parsedFormula);
  }
  return std::move(s[worker]);
}
void
CaDiCaLManager::returnHandleFromWorker(CaDiCaLHandlePtr handle,
                                       parac_worker worker) {
  auto& s = m_internal->solvers;
  auto& b = m_internal->borrowed;

  assert(worker < s.size());
  assert(b[worker]);

  b[worker] = false;

  s[worker] = std::move(handle);
}

SolverTask*
CaDiCaLManager::createSolverTask(
  parac_task& task,
  std::shared_ptr<cubesource::Source> cubesource) {
  std::unique_lock lock(Internal::solverTasksMutex);
  auto& wrapper = Internal::solverTasks.emplace_front();
  wrapper.t.init(*this, task, cubesource);
  wrapper.it = Internal::solverTasks.begin();
  return &wrapper.t;
}
void
CaDiCaLManager::deleteSolverTask(SolverTask* task) {
  assert(task);
  Internal::SolverTaskWrapper* taskWrapper =
    reinterpret_cast<Internal::SolverTaskWrapper*>(
      reinterpret_cast<std::byte*>(task) -
      offsetof(Internal::SolverTaskWrapper, t));
  assert(taskWrapper);
  std::unique_lock lock(Internal::solverTasksMutex);
  Internal::solverTasks.erase(taskWrapper->it);
}

CaDiCaLManager::CaDiCaLHandlePtrWrapper::CaDiCaLHandlePtrWrapper(
  CaDiCaLHandlePtr ptr,
  CaDiCaLManager& mgr,
  parac_worker worker)
  : ptr(std::move(ptr))
  , mgr(mgr)
  , worker(worker) {}

CaDiCaLManager::CaDiCaLHandlePtrWrapper::~CaDiCaLHandlePtrWrapper() {
  mgr.returnHandleFromWorker(std::move(ptr), worker);
}

CaDiCaLManager::CaDiCaLHandlePtrWrapper
CaDiCaLManager::getHandleForWorker(parac_worker worker) {
  return CaDiCaLHandlePtrWrapper(takeHandleForWorker(worker), *this, worker);
}

void
CaDiCaLManager::handleSatisfyingAssignmentFound(
  std::unique_ptr<SolverAssignment> assignment) {
  std::unique_lock lock(m_internal->satAssignmentMutex, std::try_to_lock);
  if(!lock.owns_lock() || m_solverAssignment) {
    // A result was already found! Do nothing with the assignment.
    return;
  }

  m_solverAssignment = std::move(assignment);

  if(originatorId() == m_mod.handle->id) {
    // Local solution found! Give solution to handle and exit paracooba, so that
    // it can be printed in main().

    mod().handle->assignment_data = m_solverAssignment.get();
    mod().handle->assignment_is_set = &SolverAssignment::static_isSet;
    mod().handle->assignment_highest_literal =
      &SolverAssignment::static_highestLiteral;

    mod().handle->request_exit(mod().handle);
  } else {
    auto& o = m_internal->satAssignmentOstream;
    assert(!o);
    o = std::make_unique<NoncopyOStringstream>();

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "SAT result destined for originator {} found in daemon compute "
              "node! Encoding and sending.",
              originatorId());

    {
      cereal::BinaryOutputArchive oa(*o);
      oa(*m_solverAssignment);
    }

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "SAT result destined for originator {} encoded into {}B.",
              originatorId(),
              o->tellp());

    // Assignment found on some worker node! Send to master.
    auto brokerMod = mod().handle->modules[PARAC_MOD_BROKER];
    assert(brokerMod);
    auto broker = brokerMod->broker;
    assert(broker);
    auto computeNodeStore = broker->compute_node_store;
    assert(computeNodeStore);
    assert(computeNodeStore->has(computeNodeStore, originatorId()));

    auto originator = computeNodeStore->get(computeNodeStore, originatorId());
    assert(originator);
    assert(originator->available_to_send_to(originator));

    parac_message_wrapper msg;
    msg.kind = PARAC_MESSAGE_SOLVER_SAT_ASSIGNMENT;
    msg.originator_id = originatorId();
    msg.data = o->ptr();
    msg.length = o->tellp();

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Sending SAT result destined for originator {}.",
              originatorId());

    originator->send_message_to(originator, &msg);
  }
}

CubeIteratorRange
CaDiCaLManager::getCubeFromPath(parac_path path) const {
  return m_parsedFormula->getCubeFromPath(path);
}

parac_id
CaDiCaLManager::originatorId() const {
  return m_parsedFormula->originatorId();
}

std::unique_ptr<cubesource::Source>
CaDiCaLManager::createRootCubeSource() {
  if(m_solverConfig.PredefinedCubes()) {
    return std::make_unique<cubesource::PathDefined>();
  } else {
    return std::make_unique<cubesource::Unspecified>();
  }
}
const CaDiCaLHandle&
CaDiCaLManager::parsedFormulaHandle() const {
  return *m_parsedFormula;
}

void
CaDiCaLManager::updateAverageSolvingTime(double ms) const {
  auto& avg = m_internal->acc_averageSolvingTime;
  std::unique_lock lock(m_internal->averageSolvingTimeMutex);
  avg(ms);
}
double
CaDiCaLManager::averageSolvingTimeMS() {
  std::unique_lock lock(m_internal->averageSolvingTimeMutex);

  auto duration =
    boost::accumulators::rolling_mean(m_internal->acc_averageSolvingTime);
  return duration < 1000 ? 1000 : duration;
}

void
CaDiCaLManager::addWaitingSolverTask() {
  ++m_internal->waitingSolverTasks;
}
void
CaDiCaLManager::removeWaitingSolverTask() {
  --m_internal->waitingSolverTasks;
}
size_t
CaDiCaLManager::waitingSolverTasks() const {
  return m_internal->waitingSolverTasks;
}
}
