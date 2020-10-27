#include <cassert>

#include "cadical_handle.hpp"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "parser_task.hpp"

#include <paracooba/common/task.h>

namespace parac::solver {
struct ParserTask::Internal {
  Internal(bool& stop)
    : handlePtr(std::make_unique<CaDiCaLHandle>(stop)) {}
  CaDiCaLHandlePtr handlePtr;
};

ParserTask::ParserTask(parac_task& task,
                       const std::string& file,
                       FinishedCB finishedCB)
  : m_internal(std::make_unique<Internal>(task.stop))
  , m_task(task)
  , m_file(file)
  , m_finishedCB(finishedCB) {
  task.state = PARAC_TASK_NEW;
  task.result = PARAC_UNDEFINED;
  task.left_result = PARAC_UNDEFINED;
  task.right_result = PARAC_UNDEFINED;
  task.path.rep = PARAC_PATH_EXPLICITLY_UNKNOWN;
  task.received_from = 0;
  task.offloaded_to = 0;
  task.userdata = this;
  task.work = &ParserTask::work;
}
ParserTask::~ParserTask() {}

parac_task_state
ParserTask::assess(parac_task* self) {
  assert(self);
  assert(self->userdata);
  ParserTask* t = static_cast<ParserTask*>(self->userdata);
  if(!t->m_internal->handlePtr->hasFormula()) {
    return PARAC_TASK_WORK_AVAILABLE;
  }
  return PARAC_TASK_DONE;
}

parac_status
ParserTask::work(parac_task* self) {
  assert(self);
  assert(self->userdata);
  ParserTask* t = static_cast<ParserTask*>(self->userdata);
  assert(!t->m_internal->handlePtr->hasFormula());
  parac_status s = t->m_internal->handlePtr->parseFile(t->m_file);
  if(t->m_finishedCB) {
    t->m_finishedCB(s, std::move(t->m_internal->handlePtr));
  }
  return s;
}
}
