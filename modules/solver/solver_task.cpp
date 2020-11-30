#include <cassert>

#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/common/task_store.h"
#include "paracooba/common/types.h"
#include "paracooba/module.h"
#include "paracooba/solver/cube_iterator.hpp"
#include "solver_assignment.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"

#include <chrono>
#include <paracooba/common/log.h>
#include <paracooba/common/task.h>
#include <thread>

#include <mutex>
#include <set>

namespace parac::solver {
class FastSplit {
  public:
  operator bool() const { return fastSplit; }
  void half_tick(bool b) { local_situation = b; }
  void tick(bool b) {
    assert(beta <= alpha);
    const bool full_tick = (b || local_situation);
    if(full_tick)
      ++beta;
    else
      fastSplit = false;
    ++alpha;

    if(alpha == period) {
      fastSplit = (beta >= period / 2);
      beta = 0;
      alpha = 0;
      if(fastSplit)
        ++depth;
      else
        --depth;
    }
  }
  int split_depth() { return fastSplit ? depth : 0; }

  private:
  unsigned alpha = 0;
  unsigned beta = 0;
  bool fastSplit = true;
  const unsigned period = 8;
  int depth = 1;
  bool local_situation = true;
};

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
}

parac_status
SolverTask::work(parac_worker worker) {
  auto handlePtr = m_manager->getHandleForWorker(worker);
  auto& handle = *handlePtr.ptr;

  {
    static std::mutex mtx;
    static std::set<parac_path_type> paths;
    std::unique_lock lock(mtx);
    assert(!paths.count(m_task->path.rep));
    paths.insert(m_task->path.rep);
  }

  assert(m_manager);
  assert(m_task);
  assert(m_task->task_store);
  assert(m_cubeSource);

  bool split_left, split_right;

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
      parac_task* l = m_task->task_store->new_task(
        m_task->task_store, m_task, parac_path_get_next_left(m_task->path));
      create(*l, *m_manager, m_cubeSource);
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
      parac_task* r = m_task->task_store->new_task(
        m_task->task_store, m_task, parac_path_get_next_right(m_task->path));
      create(*r, *m_manager, m_cubeSource);
      m_task->right_result = PARAC_PENDING;
      m_task->task_store->assess_task(m_task->task_store, r);
    } else {
      m_task->right_result = PARAC_UNSAT;
    }

    return PARAC_PENDING;
  } else {
    m_task->state = PARAC_TASK_DONE;
    auto cube = m_cubeSource->cube(m_task->path, *m_manager);
    handle.applyCubeAsAssumption(cube);
    parac_status s = handle.solve();

    if(s == PARAC_SAT) {
      m_manager->handleSatisfyingAssignmentFound(handle.takeSolverAssignment());
    }
    return s;
  }
}
parac_status
SolverTask::serialize_to_msg(parac_message* tgt_msg) {
  assert(tgt_msg);
  return PARAC_OK;
}

SolverTask&
SolverTask::createRoot(parac_task& task, CaDiCaLManager& manager) {
  auto self = manager.createSolverTask(task, manager.createRootCubeSource());
  assert(self);
  task.path = Path::build(0, 0);
  return *self;
}

SolverTask&
SolverTask::create(parac_task& task,
                   CaDiCaLManager& manager,
                   std::shared_ptr<cubesource::Source> source) {
  assert(source);

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
}
