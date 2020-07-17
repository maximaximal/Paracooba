#ifndef PARACOOBA_TASKRESULT_HPP
#define PARACOOBA_TASKRESULT_HPP

#include <memory>

#include "types.hpp"

namespace paracooba {
class Task;

/** @brief This class holds the result of a task. This can be a success message,
 * an assignment, a cube, or other results.
 *
 * The original task is also contained,
 * in case some of it can be reused.
 */
class TaskResult
{
  public:
  /** @brief The status code task can result in. This affects execution of other
   * tasks.
   */
  enum Status
  {
    Success,
    DecisionMade,
    NoDecisionMade,
    CreatedCaDiCaLTask,
    MissingInputs,
    ParsingError,
    PathAlreadyVisitedError,

    Satisfiable,
    Unsolved,
    Unsatisfiable,
    Resplitted,
    Parsed,
    Unknown,
    FileNotFound
  };

  /** @brief Create a task result with an assigned status. */
  TaskResult(Status status);
  /** @brief Destructor */
  ~TaskResult();

  /** @brief Get the status of this task. */
  Status getStatus() const { return m_status; }
  /** @brief Return the task that produced this result.
   *
   * The task is deleted with this task result, because the result owns the task
   * after it has finished. */
  Task& getTask() const { return *m_task; }

  /** @brief Return the internal unique pointer to the task that produced this
   * result.
   *
   * Once this has been moved again, no other call can use the internal task!
   */
  std::unique_ptr<Task>& getTaskPtr() { return m_task; }

  private:
  friend class Runner;

  Status m_status;
  std::unique_ptr<Task> m_task;

  void setTask(std::unique_ptr<Task> task);
};

using TaskResultPtr = std::unique_ptr<TaskResult>;
}

#endif
