#include <cassert>

#include "parser_qbf.hpp"
#include "qbf_parser_task.hpp"

namespace parac::solver_qbf {
QBFParserTask::QBFParserTask(parac_handle& handle,
                             parac_task& task,
                             std::string_view input,
                             FinishedCB finishedCB)
  : m_handle(handle)
  , m_task(task)
  , m_finishedCB(std::move(finishedCB))
  , m_parser(std::make_unique<Parser>()) {
  m_parser->prepare(input);

  task.work = &static_work;
  task.userdata = this;
  task.serialize = nullptr;
  task.free_userdata = [](parac_task* t) {
    assert(t);
    assert(t->userdata);
    QBFParserTask* self = static_cast<QBFParserTask*>(t->userdata);
    delete self;
    t->userdata = nullptr;
    return PARAC_OK;
  };
  task.terminate = nullptr;

  task.path.rep = PARAC_PATH_PARSER;
  task.state = PARAC_TASK_WORK_AVAILABLE;
  task.received_from = 0;
  task.offloaded_to = 0;
}
QBFParserTask::~QBFParserTask() {}

parac_status
QBFParserTask::static_work(parac_task* self, parac_worker worker) {
  assert(self);
  assert(self->userdata);

  QBFParserTask* parser_task = static_cast<QBFParserTask*>(self->userdata);
  return parser_task->work(worker);
}
parac_status
QBFParserTask::work(parac_worker worker) {
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Starting work on parser task for QBF file \"{}\"",
            m_parser->path());
  parac_status status = m_parser->parse();
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Finished work on parser task for QBF file \"{}\"",
            m_parser->path());

  if(m_finishedCB) {
    m_finishedCB(status, std::move(m_parser));
  }
  return status;
}
}
