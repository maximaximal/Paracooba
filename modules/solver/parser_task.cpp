#include <cassert>

#include "cadical_handle.hpp"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/common/types.h"
#include "parser_task.hpp"

#include <paracooba/common/task.h>

namespace parac::solver {
struct ParserTask::Internal {
  Internal(bool& stop, parac_id originatorId)
    : handlePtr(std::make_unique<CaDiCaLHandle>(stop, originatorId)) {}
  CaDiCaLHandlePtr handlePtr;
};

ParserTask::ParserTask(parac_task& task,
                       const std::string& input,
                       parac_id originatorId,
                       FinishedCB finishedCB)
  : m_internal(std::make_unique<Internal>(task.stop, originatorId))
  , m_task(task)
  , m_input(input)
  , m_finishedCB(finishedCB) {
  task.path.rep = PARAC_PATH_PARSER;
  task.state = task.state | PARAC_TASK_WORK_AVAILABLE;
  task.received_from = 0;
  task.offloaded_to = 0;
  task.userdata = this;
  task.work = &ParserTask::work;
  task.free_userdata = [](parac_task* task) {
    assert(task);
    assert(task->userdata);
    ParserTask* parserTask = static_cast<ParserTask*>(task->userdata);
    delete parserTask;
    return PARAC_OK;
  };
}
ParserTask::~ParserTask() {}

parac_status
ParserTask::work(parac_task* self, parac_worker worker) {
  (void)worker;
  assert(self);
  assert(self->userdata);
  ParserTask* t = static_cast<ParserTask*>(self->userdata);
  assert(t->m_internal);
  assert(t->m_internal->handlePtr);
  assert(!t->m_internal->handlePtr->hasFormula());
  parac_status s;
  if(t->m_input[0] == ':') {
    std::string_view view(t->m_input);
    auto contentView = view.substr(1, std::string_view::npos);
    s = t->m_internal->handlePtr->parseString(contentView);
  } else {
    s = t->m_internal->handlePtr->parseFile(t->m_input);
  }
  if(t->m_finishedCB) {
    t->m_finishedCB(s, std::move(t->m_internal->handlePtr));
  }
  return s;
}
}
