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
#include "paracooba/common/compute_node.h"
#include "paracooba/common/noncopy_ostream.hpp"
#include "paracooba/common/status.h"
#include "sat_handler.hpp"
#include "solver_assignment.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"

#include <paracooba/common/log.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>
#include <paracooba/solver/cube_iterator.hpp>

#include <paracooba/common/message.h>
#include <paracooba/common/message_kind.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>

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

  std::mutex knownPeersMutex;
  std::set<parac_compute_node*> knownPeers;

  std::mutex learnedClausesMutex;
  std::vector<Clause> learnedClauses;
  std::atomic_size_t learnedClausesCount = 0;
  parac_id learnedClausesInitiallyFrom = 0;

  std::vector<size_t> learnedClausesApplicationStack;
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
    if(startLiterals.size() != solverConfig.ConcurrentCubeTreeCount()) {
      parac_log(
        PARAC_SOLVER,
        PARAC_LOCALERROR,
        "Cannot generate {} cubing roots from formula {} with originator id "
        "{}! Only generated {} ({}). Returning with 0 as root!",
        solverConfig.ConcurrentCubeTreeCount(),
        handle.path(),
        handle.originatorId(),
        fmt::join(startLiterals, ", "),
        startLiterals.size());
      return { 0 };
    }
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
      PARAC_DEBUG,
      "Generate CaDiCaLManager for formula in file \"{}\" from compute node {} "
      "for {} "
      "workers. Copy operation is deferred to when a solver is requested.",
      m_parsedFormula->path(),
      m_parsedFormula->originatorId(),
      workers);

    ObjectManager<CaDiCaLHandle>::init(
      workers, [this](size_t idx) { return createHandle(idx); });

    m_internal->learnedClausesApplicationStack.resize(workers);
    std::fill(m_internal->learnedClausesApplicationStack.begin(),
              m_internal->learnedClausesApplicationStack.end(),
              0);
  } else {
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
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
CaDiCaLManager::createHandle(size_t idx) {
  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Creating copy of root formula in file \"{}\" from {} for worker "
            "{} because a "
            "solver object was requested from CaDiCaLManager.",
            m_parsedFormula->path(),
            m_parsedFormula->originatorId(),
            idx);
  return std::make_unique<CaDiCaLHandle>(*m_parsedFormula);
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

CaDiCaLManager::CaDiCaLHandlePtrWrapper
CaDiCaLManager::getHandleForWorker(parac_worker worker) {
  auto handle = get(worker);

  // Check if there are new clauses to apply.
  if(m_internal->learnedClausesCount >
     m_internal->learnedClausesApplicationStack[worker]) {
    std::vector<Clause> clausesToLearn;
    {
      std::unique_lock lock(m_internal->learnedClausesMutex);
      std::copy(m_internal->learnedClauses.begin() +
                  m_internal->learnedClausesApplicationStack[worker],
                m_internal->learnedClauses.end(),
                std::back_inserter(clausesToLearn));
      m_internal->learnedClausesApplicationStack[worker] =
        m_internal->learnedClauses.size();
    }

    for(Clause& clause : clausesToLearn) {
      parac_log(
        PARAC_SOLVER,
        PARAC_TRACE,
        "Apply learned clause ({}) to solver worker {} from file \"{}\" "
        "with originator id {}.",
        fmt::join(clause, " | "),
        worker,
        m_parsedFormula->path(),
        m_parsedFormula->originatorId());
      assert(clause.size() > 0);

      handle->applyLearnedClause(clause);
    }
  }

  return handle;
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

template<typename T>
void
DistributeLearnedClausesToTargets(const std::vector<Clause>& clauses,
                                  const T& targets,
                                  parac_id originatorId) {
  assert(clauses.size() >= 1);

  for(auto& c : clauses) {
    assert(c.size() > 0);
  }

  parac_message_wrapper msg;
  msg.kind = PARAC_MESSAGE_SOLVER_NEW_LEARNED_CLAUSE;
  msg.userdata = new int;

  // Count the references to the data of this message.
  int* count = static_cast<int*>(msg.userdata);
  *count = 1;

  {
    NoncopyOStringstream os;
    {
      cereal::BinaryOutputArchive oa(os);
      oa(clauses);
    }
    msg.data = new char[os.tellp()];
    msg.length = os.tellp();
    msg.originator_id = originatorId;
    std::copy(os.ptr(), os.ptr() + os.tellp(), msg.data);
  }

  msg.cb = [](parac_message* msg, parac_status s) {
    if(s == PARAC_TO_BE_DELETED) {
      int* count = static_cast<int*>(msg->userdata);
      if(--(*count) == 0) {
        delete[] msg->data;
        msg->data = nullptr;
        delete count;
      }
    }
  };

  {
    for(parac_compute_node* peer : targets) {
      assert(peer);
      if(peer->available_to_send_to(peer)) {
        ++(*count);
        peer->send_message_to(peer, &msg);

        if(parac_log_enabled(PARAC_SOLVER, PARAC_TRACE)) {
          std::vector<std::string> clausesString;
          std::transform(clauses.begin(),
                         clauses.end(),
                         std::back_inserter(clausesString),
                         [](const Clause& c) {
                           return fmt::format("({})", fmt::join(c, "|"));
                         });

          parac_log(PARAC_SOLVER,
                    PARAC_TRACE,
                    "Distributing learned clauses {} with originator {} to "
                    "peer with id {}.",
                    fmt::join(clausesString, ", "),
                    originatorId,
                    peer->id);
        }
      }
    }
  }

  msg.cb(&msg, PARAC_TO_BE_DELETED);
}

void
CaDiCaLManager::addPossiblyNewNodePeer(parac_compute_node& peer) {
  bool inserted = false;
  {
    std::unique_lock lock(m_internal->knownPeersMutex);
    inserted = m_internal->knownPeers.insert(&peer).second;
  }

  if(inserted && peer.id != m_internal->learnedClausesInitiallyFrom) {
    // Distribute all previously known clauses to the peer too!
    std::vector<Clause> learnedClauses;

    {
      std::unique_lock lock(m_internal->knownPeersMutex);
      learnedClauses = m_internal->learnedClauses;
    }

    if(learnedClauses.size() == 0)
      return;

    DistributeLearnedClausesToTargets(
      learnedClauses, std::set<parac_compute_node*>{ &peer }, originatorId());
  }
}

void
CaDiCaLManager::applyAndDistributeNewLearnedClause(Clause c, parac_id source) {
  assert(c.size() > 0);

  // Add to internal solver queue.
  {
    std::unique_lock lock(m_internal->learnedClausesMutex);
    m_internal->learnedClauses.emplace_back(c);
    m_internal->learnedClausesCount = m_internal->learnedClauses.size();
  }

  // Send to known peers.
  if(source == 0) {
    std::set<parac_compute_node*> peers;
    {
      std::unique_lock lock(m_internal->knownPeersMutex);
      peers = m_internal->knownPeers;
    }
    DistributeLearnedClausesToTargets({ c }, peers, originatorId());
  } else {
    if(m_internal->learnedClausesInitiallyFrom == 0) {
      m_internal->learnedClausesInitiallyFrom = source;
    }
  }
}
}
