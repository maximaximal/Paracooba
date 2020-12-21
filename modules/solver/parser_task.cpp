#include <boost/filesystem/operations.hpp>
#include <cassert>

#include "cadical_handle.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/common/types.h"
#include "parser_task.hpp"

#include <paracooba/common/task.h>
#include <paracooba/module.h>

namespace parac::solver {
struct ParserTask::Internal {
  Internal(parac_handle& handle, bool& stop, parac_id originatorId)
    : handle(handle)
    , handlePtr(std::make_unique<CaDiCaLHandle>(handle, stop, originatorId)) {}
  parac_handle& handle;
  CaDiCaLHandlePtr handlePtr;
};

ParserTask::ParserTask(parac_handle& handle,
                       parac_task& task,
                       std::string input,
                       parac_id originatorId,
                       FinishedCB finishedCB)
  : m_internal(std::make_unique<Internal>(handle, task.stop, originatorId))
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

  if(m_input[0] == ':') {
    std::string_view view(m_input);
    auto contentView = view.substr(1, std::string_view::npos);
    auto [status, outFile] = m_internal->handlePtr->prepareString(contentView);
    m_path = outFile;
  } else {
    m_path = m_input;
  }
}
ParserTask::~ParserTask() {}

parac_status
ParserTask::work(parac_task* self, parac_worker worker) {
  (void)worker;
  assert(self);
  assert(self->userdata);
  ParserTask* t = static_cast<ParserTask*>(self->userdata);

  if(!boost::filesystem::exists(t->m_path)) {
    parac_log(PARAC_SOLVER,
              PARAC_GLOBALERROR,
              "Could not find formula to parse in path {}!",
              t->m_path);
    return PARAC_FILE_NOT_FOUND_ERROR;
  }

  assert(t->m_internal);
  assert(t->m_internal->handlePtr);
  assert(!t->m_internal->handlePtr->hasFormula());
  parac_status s;
  s = t->m_internal->handlePtr->parseFile(t->m_path);
  if(t->m_finishedCB) {
    t->m_finishedCB(s, std::move(t->m_internal->handlePtr));
  }
  return s;
}
}
