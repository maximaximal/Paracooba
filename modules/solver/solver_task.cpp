#include <cassert>

#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "paracooba/common/types.h"
#include "paracooba/solver/cube_iterator.hpp"
#include "solver_assignment.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"

#include <chrono>
#include <paracooba/common/task.h>
#include <paracooba/common/log.h>
#include <thread>

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
SolverTask::init(CaDiCaLManager& manager, parac_task& task) {
  m_manager = &manager;
  m_task = &task;

  task.state = task.state | PARAC_TASK_WORK_AVAILABLE;
  task.work = &static_work;
  task.serialize = &static_serialize;
  task.userdata = this;
}

parac_status
SolverTask::work(parac_worker worker) {
  auto handlePtr = m_manager->getHandleForWorker(worker);
  auto& handle = handlePtr.ptr;

  parac_status s = handle->solve();

  if(s == PARAC_SAT) {
    m_manager->handleSatisfyingAssignmentFound(handle->takeSolverAssignment());
  }
  return s;
}
parac_status
SolverTask::serialize_to_msg(parac_message* tgt_msg) {
  assert(tgt_msg);
  return PARAC_OK;
}

SolverTask&
SolverTask::createRoot(parac_task& task, CaDiCaLManager& manager) {
  auto self = manager.createSolverTask(task);
  assert(self);
  task.path = Path::build(0, 0);
  return *self;
}

SolverTask&
SolverTask::create(parac_task& task,
                   CaDiCaLManager& manager,
                   cubesource::Source& source) {
  auto self = manager.createSolverTask(task);
  assert(self);
  manager.createSolverTask(task);
  self->m_cubeSource = &source;
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
