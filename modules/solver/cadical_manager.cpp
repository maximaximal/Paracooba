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
#include "sat_handler.hpp"
#include "solver_assignment.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"

#include <paracooba/common/log.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>
#include <paracooba/solver/cube_iterator.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>

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

  std::vector<std::shared_ptr<cubesource::Source>> rootCubeSources;
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

static std::vector<Literal>
MakeStartLiteralVector(CaDiCaLHandle& handle, SolverConfig& solverConfig) {
  std::vector<Literal> startLiterals;
  assert(solverConfig.ConcurrentCubeTreeCount() >= 1);
  if(solverConfig.ConcurrentCubeTreeCount() == 1) {
    startLiterals.emplace_back(0);
  } else if(solverConfig.ConcurrentCubeTreeCount() > 1) {
    CaDiCaLHandle::FastLookaheadResult r =
      handle.fastLookahead(solverConfig.ConcurrentCubeTreeCount());
    assert(r.status == PARAC_SPLITTED);
    assert(r.cubes.size() >= 1);
    startLiterals = r.cubes[0];
    for(Literal& l : startLiterals) {
      l = std::abs(l);
    }
  }
  return startLiterals;
}

CaDiCaLManager::CaDiCaLManager(parac_module& mod,
                               CaDiCaLHandlePtr parsedFormula,
                               SolverConfig& solverConfig,
                               SatHandler& satHandler)
  : m_internal(std::make_unique<Internal>(getAvailableWorkersFromMod(mod)))
  , m_mod(mod)
  , m_parsedFormula(std::move(parsedFormula))
  , m_solverConfig(solverConfig)
  , m_satHandler(satHandler) {

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

  } else {
    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "Generate dummy CaDiCaLManager for formula that was not parsed "
              "locally, as there are 0 workers.");
  }

  if(mod.handle->input_file) {
    if(solverConfig.CaDiCaLCubes()) {
      std::vector<Literal> startLiterals{ 0 };

      if(m_parsedFormula) {
        startLiterals = MakeStartLiteralVector(*m_parsedFormula, solverConfig);
        assert(startLiterals.size() >= 1);
        parac_log(
          PARAC_SOLVER,
          PARAC_TRACE,
          "Because --cadical-cubes is active, produced start literal "
          "vector containing {} literals to start ({}). Literals are expanded "
          "in worker thread.",
          startLiterals.size(),
          fmt::join(startLiterals, ", "));
      } else {
        parac_log(PARAC_SOLVER,
                  PARAC_LOCALWARNING,
                  "The client has no local workers but --cadical-cubes is "
                  "active! Cubes will be expanded on peers, but only one "
                  "parallel cube tree is possible.");
      }

      uint32_t concurrentCubeTreeNumber = 0;
      std::transform(startLiterals.begin(),
                     startLiterals.end(),
                     std::back_inserter(m_internal->rootCubeSources),
                     [&concurrentCubeTreeNumber](Literal lit) {
                       return std::make_shared<cubesource::CaDiCaLCubes>(
                         lit, concurrentCubeTreeNumber++);
                     });
    } else {
      m_internal->rootCubeSources = {
        std::make_shared<cubesource::PathDefined>()
      };
    }
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
  CaDiCaLHandlePtrWrapper wrapper(takeHandleForWorker(worker), *this, worker);
  return wrapper;
}

void
CaDiCaLManager::handleSatisfyingAssignmentFound(
  std::unique_ptr<SolverAssignment> assignment) {
  m_satHandler.handleSatisfyingAssignmentFound(std::move(assignment));
}

CubeIteratorRange
CaDiCaLManager::getCubeFromPath(parac_path path) const {
  return m_parsedFormula->getCubeFromPath(path);
}

parac_id
CaDiCaLManager::originatorId() const {
  return m_parsedFormula->originatorId();
}

const std::vector<std::shared_ptr<cubesource::Source>>&
CaDiCaLManager::getRootCubeSources() {
  assert(m_internal->rootCubeSources.size() > 0);
  return m_internal->rootCubeSources;
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
parac_id
CaDiCaLManager::getOriginatorId() const {
  return m_solverConfig.OriginatorId();
}
}
