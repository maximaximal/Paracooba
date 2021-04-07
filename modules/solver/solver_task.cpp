#include <cassert>

#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "cube_source.hpp"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/noncopy_ostream.hpp"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/common/task_store.h"
#include "paracooba/common/types.h"
#include "paracooba/module.h"
#include "paracooba/solver/cube_iterator.hpp"
#include "paracooba/solver/solver.h"
#include "parser_task.hpp"
#include "solver_assignment.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"

#include <memory>
#include <paracooba/communicator/communicator.h>
#include <paracooba/runner/runner.h>

#include <chrono>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/common/task.h>
#include <thread>

#include <cereal/archives/binary.hpp>

#include <mutex>
#include <set>

namespace parac::solver {
SolverTask::SolverTask() {}

SolverTask::~SolverTask() {}

void
SolverTask::init(CaDiCaLManager& manager,
                 parac_task& task,
                 std::shared_ptr<cubesource::Source> cubesource) {
  m_manager = &manager;
  m_task = &task;
  m_cubeSource = cubesource;

  task.state = PARAC_TASK_WORK_AVAILABLE;
  task.work = &static_work;
  task.serialize = &static_serialize;
  task.userdata = this;
  task.terminate = &static_terminate;

  task.free_userdata = [](parac_task* t) {
    assert(t);
    assert(t->userdata);
    SolverTask* solverTask = static_cast<SolverTask*>(t->userdata);
    CaDiCaLManager* manager = solverTask->manager();
    assert(manager);
    manager->deleteSolverTask(solverTask);
    return PARAC_OK;
  };

  manager.addWaitingSolverTask();
}

static parac_timeout*
setTimeout(CaDiCaLManager& manager,
           uint64_t ms,
           void* userdata,
           parac_timeout_expired expired_cb) {
  parac_handle* paracHandle = manager.mod().handle;
  assert(paracHandle);
  parac_module* paracCommMod = paracHandle->modules[PARAC_MOD_COMMUNICATOR];
  if(!paracCommMod)
    return nullptr;
  parac_module_communicator* paracComm = paracCommMod->communicator;
  if(!paracComm)
    return nullptr;

  return paracComm->set_timeout(paracCommMod, ms, userdata, expired_cb);
}

parac_status
SolverTask::work(parac_worker worker) {
  auto handlePtr = m_manager->getHandleForWorker(worker);
  auto& handle = *handlePtr.ptr;

  m_activeHandle = &handle;

  assert(m_manager);
  assert(m_task);
  assert(m_task->task_store);
  assert(m_cubeSource);

  m_manager->removeWaitingSolverTask();

  bool split_left, split_right;

  parac_status s;

  if(m_cubeSource->split(
       m_task->path, *m_manager, handle, split_left, split_right)) {
    // The split may already be known from the cube source (think of predefined
    // cubes). In this case, just split the task and create new tasks.

    m_task->state =
      PARAC_TASK_SPLITTED | PARAC_TASK_WAITING_FOR_SPLITS | PARAC_TASK_DONE;

    if(split_left) {
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Task on path {} lsplit to path {}",
                m_task->path,
                parac_path_get_next_left(m_task->path));
      assert(m_task->task_store);
      parac_task* l =
        m_task->task_store->new_task(m_task->task_store,
                                     m_task,
                                     parac_path_get_next_left(m_task->path),
                                     m_task->originator);
      create(*l, *m_manager, m_cubeSource->leftChild(m_cubeSource));
      assert(m_task->task_store);
      m_task->left_result = PARAC_PENDING;
      m_task->task_store->assess_task(m_task->task_store, l);
    } else {
      m_task->left_result = PARAC_UNSAT;
    }

    if(split_right) {
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Task on path {} rsplit to path {}",
                m_task->path,
                parac_path_get_next_right(m_task->path));
      parac_task* r =
        m_task->task_store->new_task(m_task->task_store,
                                     m_task,
                                     parac_path_get_next_right(m_task->path),
                                     m_task->originator);
      create(*r, *m_manager, m_cubeSource->rightChild(m_cubeSource));
      m_task->right_result = PARAC_PENDING;
      m_task->task_store->assess_task(m_task->task_store, r);
    } else {
      m_task->right_result = PARAC_UNSAT;
    }

    s = PARAC_PENDING;
  } else {
    auto& config = m_manager->config();
    uint64_t averageSolvingTime = m_manager->averageSolvingTimeMS();
    m_fastSplit.tick(m_manager->waitingSolverTasks() <= 1);
    const float multiplicationFactor = m_fastSplit ? 0.5 : 2;
    uint64_t duration = multiplicationFactor * averageSolvingTime;

    auto cube = m_cubeSource->cube(m_task->path, *m_manager);
    handle.applyCubeAsAssumption(cube);

    s = PARAC_UNKNOWN;
    uint64_t solving_duration = 0;
    parac_status splitting_status = PARAC_PENDING;
    std::vector<CubeTreeElem> splitting_cubes;
    uint64_t splitting_duration = 0;

    if(config.Resplit()) {
      parac_log(PARAC_CUBER,
                PARAC_TRACE,
                "Start solving CNF formula on path {} using CaDiCaL CNF solver "
                "with cube {} as assumption "
                "for roughly "
                "{}ms before aborting it (fastSplit = {}, "
                "averageSolvingTimeMS: {}, waitingSolverTasks: {})",
                path(),
                cube,
                duration,
                m_fastSplit,
                averageSolvingTime,
                m_manager->waitingSolverTasks());
      splitting_status = PARAC_PENDING;
      auto r = solveOrConditionallyAbort(config, handle, duration);
      s = r.first;
      solving_duration = r.second;
    } else {
      parac_log(PARAC_CUBER,
                PARAC_TRACE,
                "Start solving CNF formula on path {} and originator id {} "
                "with cube {} as "
                "assumption using CaDiCaL CNF "
                "solver. No resplitting is carried out here.",
                path(),
                handle.originatorId(),
                cube);
      splitting_status = PARAC_PENDING;
      auto r = solveOrConditionallyAbort(config, handle, 0);
      s = r.first;
      solving_duration = r.second;
    }

    if(m_interruptSolving) {
      switch(s) {
        case PARAC_ABORTED: {
          if(splitting_status == PARAC_PENDING) {
            auto [resplit_status, cubes, resplit_duration] = resplit(duration);
            splitting_cubes = std::move(cubes);
            splitting_status = resplit_status;
            splitting_duration = resplit_duration;
          }

          if(splitting_status == PARAC_SPLITTED) {
            m_task->result = PARAC_PENDING;
            for(auto [task, _] : splitting_cubes) {
              s = PARAC_PENDING;
              task->task_store->assess_task(task->task_store, task);
            }
          } else {
            s = splitting_status;
          }

          m_manager->updateAverageSolvingTime(solving_duration +
                                              splitting_duration);

          break;
        }
        case PARAC_SAT:
          m_manager->updateAverageSolvingTime(duration);
          break;
        case PARAC_UNSAT:
          m_manager->updateAverageSolvingTime(duration);
          break;
        default:
          s = PARAC_UNDEFINED;
      }
    }

    if(s == PARAC_SAT || s == PARAC_UNSAT || s == PARAC_ABORTED ||
       s == PARAC_UNKNOWN) {
      m_task->state = PARAC_TASK_DONE;
    }

    if(s == PARAC_SAT) {
      m_manager->handleSatisfyingAssignmentFound(handle.takeSolverAssignment());
    }
  }

  m_activeHandle = nullptr;
  return s;
}
parac_status
SolverTask::serialize_to_msg(parac_message* tgt_msg) {
  assert(tgt_msg);
  assert(m_task);

  if(!m_serializationOutStream) {
    m_serializationOutStream = std::make_unique<NoncopyOStringstream>();

    {
      cereal::BinaryOutputArchive oa(*m_serializationOutStream);
      parac_path_type p = m_task->path.rep;
      oa(p);
      oa(reinterpret_cast<intptr_t>(m_task));
      oa(*this);
    }
  }

  tgt_msg->kind = PARAC_MESSAGE_SOLVER_TASK;
  tgt_msg->data = m_serializationOutStream->ptr();
  tgt_msg->length = m_serializationOutStream->tellp();
  tgt_msg->originator_id = m_task->originator;

  assert(m_manager);
  m_manager->removeWaitingSolverTask();

  return PARAC_OK;
}

SolverTask&
SolverTask::create(parac_task& task,
                   CaDiCaLManager& manager,
                   std::shared_ptr<cubesource::Source> source) {
  assert(source);

  task.pre_path_sorting_critereon = source->prePathSortingCritereon();

  auto self = manager.createSolverTask(task, source);
  assert(self);
  self->m_cubeSource = source;
  return *self;
}

parac_status
SolverTask::static_work(parac_task* task, parac_worker worker) {
  assert(task);
  assert(task->userdata);
  SolverTask* t = static_cast<SolverTask*>(task->userdata);
  return t->work(worker);
}

parac_status
SolverTask::static_serialize(parac_task* task, parac_message* tgt_msg) {
  assert(task);
  assert(task->userdata);
  SolverTask* t = static_cast<SolverTask*>(task->userdata);
  return t->serialize_to_msg(tgt_msg);
}

void
SolverTask::static_terminate(parac_task* task) {
  assert(task);
  assert(task->userdata);
  SolverTask* t = static_cast<SolverTask*>(task->userdata);
  if(auto activeHandle = t->m_activeHandle.load()) {
    activeHandle->terminate();
  }
  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "SolverTask in worker {} terminated!",
            task->worker);
}

const parac_path&
SolverTask::path() const {
  assert(m_task);
  return m_task->path;
}
parac_path&
SolverTask::path() {
  assert(m_task);
  return m_task->path;
}

/******************************************************************
 * RESPLITTING
 ******************************************************************/

static parac_task*
makeTaskFromCube(CaDiCaLManager* manager,
                 parac_task* parent,
                 parac_path path,
                 const Cube& cube,
                 cubesource::Source& parentSource) {
  assert(
    std::none_of(cube.begin(), cube.end(), [](Literal l) { return l == 0; }));

  parent->state =
    PARAC_TASK_SPLITTED | PARAC_TASK_WAITING_FOR_SPLITS | PARAC_TASK_DONE;

  parac_task* new_task = parent->task_store->new_task(
    parent->task_store, parent, path, parent->originator);
  assert(new_task);
  SolverTask::create(*new_task,
                     *manager,
                     std::make_shared<cubesource::Supplied>(
                       cube, parentSource.prePathSortingCritereon()));

  if(parac_path_get_next_left(parent->path) == path) {
    parent->left_result = PARAC_PENDING;
  } else if(parac_path_get_next_right(parent->path) == path) {
    parent->right_result = PARAC_PENDING;
  } else {
    assert(false);
  }

  new_task->result = PARAC_PENDING;

  return new_task;
}

static std::vector<SolverTask::CubeTreeElem>
expandLeftAndRightCube(CaDiCaLManager* manager,
                       parac_task* t,
                       parac_path p,
                       const std::pair<Cube, Cube>& c,
                       cubesource::Source& parentCubeSource) {
  assert(std::none_of(
    c.first.begin(), c.first.end(), [](Literal l) { return l == 0; }));
  assert(std::none_of(
    c.second.begin(), c.second.end(), [](Literal l) { return l == 0; }));

  return {
    { makeTaskFromCube(
        manager, t, parac_path_get_next_left(p), c.first, parentCubeSource),
      c.first },
    { makeTaskFromCube(
        manager, t, parac_path_get_next_right(p), c.second, parentCubeSource),
      c.second }
  };
}

std::pair<parac_status, std::vector<SolverTask::CubeTreeElem>>
SolverTask::resplitDepth(parac_path path, Cube literals, int depth) {
  Cube literals2(literals);
  parac_log(
    PARAC_CUBER, PARAC_TRACE, "Cubing path {} at depth {}.", path, depth);

  auto activeHandle = m_activeHandle.load();
  activeHandle->applyCubeAsAssumption(literals);

  auto [status, left_right_cube] = activeHandle->resplitOnce(path, literals);
  if(status != PARAC_SPLITTED) {
    return { status, {} };
  }

  assert(left_right_cube.has_value());

  std::vector<CubeTreeElem> cubes{ expandLeftAndRightCube(
    m_manager, m_task, m_task->path, left_right_cube.value(), *m_cubeSource) };

  auto path_depth = 1 + parac_path_length(path);
  for(int i = path_depth;
      i < depth && i < PARAC_PATH_MAX_LENGTH && !m_interruptSolving;
      ++i) {
    parac_log(PARAC_CUBER, PARAC_TRACE, "Cubing path {} at depth {}", path, i);
    auto cubes2{ std::move(cubes) };
    cubes.clear();
    for(auto&& [task, cube] : cubes2) {
      auto [status, left_and_right_cube] =
        activeHandle->resplitOnce(task->path, cube);
      if(status == PARAC_NO_SPLITS_LEFT) {
        continue;
      }
      if(status != PARAC_SPLITTED) {
        // The cube was not splitted again! This means that this path is already
        // solved!
        task->state = PARAC_TASK_DONE;
        task->result = status;
        parac_log(PARAC_CUBER,
                  PARAC_DEBUG,
                  "Cubing of path {} at depth {} led to result {}!",
                  path,
                  i,
                  status);
        continue;
      }
      assert(left_and_right_cube.has_value());
      auto pcubes = expandLeftAndRightCube(m_manager,
                                           task,
                                           task->path,
                                           left_and_right_cube.value(),
                                           *m_cubeSource);
      std::copy(pcubes.begin(), pcubes.end(), std::back_inserter(cubes));
    }
    parac_log(PARAC_CUBER,
              PARAC_TRACE,
              "CNF recubing from path {} at depth {} is finished.",
              path,
              1 + i);
  }

  return { cubes.size() > 0 ? PARAC_SPLITTED : PARAC_NO_SPLITS_LEFT, cubes };
}

std::tuple<parac_status, std::vector<SolverTask::CubeTreeElem>, uint64_t>
SolverTask::resplit(uint64_t durationMS) {
  m_interruptSolving = false;

  parac_log(PARAC_CUBER,
            PARAC_TRACE,
            "CNF formula for path {} must be resplitted. Giving it {}ms time.",
            path(),
            durationMS);

  m_timeout =
    setTimeout(*m_manager, durationMS, this, [](parac_timeout* timeout) {
      SolverTask* self = static_cast<SolverTask*>(timeout->expired_userdata);
      parac_log(PARAC_CUBER,
                PARAC_TRACE,
                "CNF lookahead for path {} will be interrupted!",
                self->path());

      auto activeHandle = self->m_activeHandle.load();
      if(activeHandle) {
        activeHandle->terminate();
        self->m_interruptSolving = true;
      }
      self->m_timeout = nullptr;
    });

  if(!m_timeout) {
    return { PARAC_ABORTED, {}, 0 };
  }

  auto start = std::chrono::steady_clock::now();
  const int depth = m_fastSplit.split_depth();

  CubeIteratorRange range = m_cubeSource->cube(path(), *m_manager);
  Cube literals(range.begin(), range.end());

  auto cubes = resplitDepth(path(), literals, depth);
  auto end = std::chrono::steady_clock::now();

  if(m_timeout) {
    m_timeout->cancel(m_timeout);
    m_timeout = nullptr;
  }

  parac_log(
    PARAC_CUBER,
    PARAC_TRACE,
    "Splitting path {} took {}ms. Produced {} sub-tasks. Returned status {}",
    m_task->path,
    std::chrono::duration<double>(
      std::chrono::duration_cast<std::chrono::milliseconds>(1'000 *
                                                            (end - start)))
      .count(),
    cubes.second.size(),
    cubes.first);

  uint64_t duration_splitting =
    std::chrono::duration_cast<std::chrono::milliseconds>(1'000 * (end - start))
      .count();

  return { cubes.first, cubes.second, duration_splitting };
}
std::pair<parac_status, uint64_t>
SolverTask::solveOrConditionallyAbort(const SolverConfig& config,
                                      CaDiCaLHandle& handle,
                                      uint64_t duration) {
  if((config.Resplit() || duration != 0)) {
    m_interruptSolving = false;
    assert(!m_timeout);
    m_timeout =
      setTimeout(*m_manager, duration, this, [](parac_timeout* timeout) {
        SolverTask* self = static_cast<SolverTask*>(timeout->expired_userdata);
        assert(self);
        parac_log(PARAC_CUBER,
                  PARAC_TRACE,
                  "CNF formula for path {} will be interrupted.",
                  self->path());
        self->m_interruptSolving = true;
        auto activeHandle = self->m_activeHandle.load();
        if(activeHandle) {
          activeHandle->terminate();
        }
        self->m_timeout = nullptr;
      });
  }

  if(m_task->stop) {
    return { PARAC_ABORTED, 0 };
  }

  m_interruptSolving = false;
  auto start = std::chrono::steady_clock::now();
  parac_status s = handle.solve();
  auto end = std::chrono::steady_clock::now();

  if(m_timeout && !m_interruptSolving) {
    m_timeout->cancel(m_timeout);
    m_timeout = nullptr;
  }

  uint64_t solverRuntimeMS =
    std::chrono::duration_cast<std::chrono::milliseconds>(1'000 * (end - start))
      .count();

  parac_log(PARAC_CUBER,
            PARAC_TRACE,
            "Stopped solving after roughly {}ms. Interrupted: {}. Status: {}",
            solverRuntimeMS,
            m_interruptSolving,
            s);

  return { s, solverRuntimeMS };
}
}
