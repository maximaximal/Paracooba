#include "parser_task.hpp"

#include <paracooba/common/task.h>

namespace parac::solver {
struct ParserTask::Internal {
  parac_task_wrapper task;
};

ParserTask::ParserTask(parac_handle& handle, const std::string& file)
  : m_internal(std::make_unique<Internal>())
  , m_handle(handle)
  , m_file(file) {}
ParserTask::~ParserTask() {}

parac_task&
ParserTask::task() {
  return m_internal->task;
}
}
