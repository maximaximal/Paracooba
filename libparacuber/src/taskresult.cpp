#include "../include/paracuber/taskresult.hpp"
#include "../include/paracuber/task.hpp"

namespace paracuber {
TaskResult::TaskResult(Status status)
  : m_status(status)
{}
TaskResult::~TaskResult() {}

void
TaskResult::setTask(std::unique_ptr<Task> task)
{
  m_task = std::move(task);
}
}
