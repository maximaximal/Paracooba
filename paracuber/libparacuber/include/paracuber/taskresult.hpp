#ifndef PARACUBER_TASKRESULT_HPP
#define PARACUBER_TASKRESULT_HPP

#include <memory>

namespace paracuber {
/** @brief This class holds the result of a task. This can be a success message,
 * an assignment, a cube, or other results.
 *
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
    MissingInputs,
    ParsingError,

    Satisfiable,
    Unsolved,
    Unsatisfiable
  };

  /** @brief Create a task result with an assigned status. */
  TaskResult(Status status)
    : m_status(status)
  {}
  /** @brief Destructor */
  ~TaskResult() {}

  /** @brief Get the status of this task. */
  Status getStatus() const { return m_status; }

  private:
  Status m_status;
};

using TaskResultPtr = std::unique_ptr<TaskResult>;
}

#endif
