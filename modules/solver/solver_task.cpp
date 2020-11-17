#include <cassert>

#include "cadical_manager.hpp"
#include "paracooba/common/types.h"
#include "solver_task.hpp"

#include <paracooba/common/task.h>

namespace parac::solver {
SolverTask::SolverTask() {}

SolverTask::~SolverTask() {}

void
SolverTask::init(CaDiCaLManager& manager, parac_task& task) {
  m_manager = &manager;
  m_task = &task;

  task.state = task.state | PARAC_TASK_WORK_AVAILABLE;
  task.work = &static_work;
  task.userdata = this;
}

parac_status
SolverTask::work(parac_worker worker) {
  return PARAC_SAT;
}

SolverTask&
SolverTask::createRoot(parac_task& task, CaDiCaLManager& manager) {
  auto self = manager.createSolverTask(task);
  assert(self);
  task.path = Path::build(0, 0);
  return *self;
}

SolverTask&
SolverTask::create(parac_task& task, CaDiCaLManager& manager) {
  auto self = manager.createSolverTask(task);
  assert(self);
  manager.createSolverTask(task);
  return *self;
}

parac_status
SolverTask::static_work(parac_task* task, parac_worker worker) {
  assert(task);
  assert(task->userdata);
  SolverTask *t = static_cast<SolverTask*>(task->userdata);
  return t->work(worker);
}
}
