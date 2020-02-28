#include "../include/paracooba/task.hpp"

namespace paracooba {
Task::Task() {}
Task::~Task() {}

void
Task::finish(const TaskResult& result)
{
  m_finishedSignal(result);
}
}
