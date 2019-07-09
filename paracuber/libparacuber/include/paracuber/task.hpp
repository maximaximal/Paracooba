#ifndef PARACUBER_TASK_HPP
#define PARACUBER_TASK_HPP

#include "log.hpp"
#include "taskresult.hpp"
#include <boost/signals2/signal.hpp>

namespace paracuber {
class Runner;
class Communicator;
class Config;

/** @brief Environment for tasks anywhere in the distributed system.
 *
 * This must be sub-classed by actual tasks to be run.
 */
class Task
{
  public:
  using FinishedSignal = boost::signals2::signal<void(const TaskResult&)>;

  /** @brief Constructor */
  Task();
  /** @brief Destructor */
  virtual ~Task();

  /** @brief Execute this task.
   *
   * Must be implemented by actual tasks. May be called multiple times to re-use
   * old task objects.
   * */
  virtual TaskResultPtr execute() = 0;

  /** @brief Returns the signal marking finished task.
   *
   * A given slot must not directly save the given reference, as it is only a
   * temporary reference to a result handled by a unique_ptr in \ref Runner (and
   * consequently in the return of \ref Task::execute).
   */
  inline FinishedSignal& getFinishedSignal() { return m_finishedSignal; }

  protected:
  friend class Runner;

  FinishedSignal m_finishedSignal;
  void finish(const TaskResult& result);

  /// Pointer to the runner that runs this task. Guaranteed to be available in
  /// execute().
  Runner* m_runner = nullptr;
  /// Pointer to the communicator around this task. Guaranteed to be available
  /// in execute().
  Communicator* m_communicator;
  /// Pointer to a valid Log instance. Guaranteed to be available in
  /// execute().
  Log* m_log = nullptr;
  /// Pointer to a valid Config instance. Guaranteed to be available in
  /// execute().
  Config* m_config = nullptr;
  // Pointer to a valid logger instance given to the task inside a worker
  // thread. Guaranteed to be available in execute(). Logs in the same context
  // as the worker thread running the task.
  Logger* m_logger = nullptr;
};
}

#endif
