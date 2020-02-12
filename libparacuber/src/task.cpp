#include "../include/paracuber/task.hpp"

namespace paracuber {
Task::Task() {}
Task::~Task() {}

void
Task::finish(const TaskResult& result)
{
  m_finishedSignal(result);
}
}
