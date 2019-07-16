#ifndef PARACUBER_RUNNER_HPP
#define PARACUBER_RUNNER_HPP

#include "log.hpp"
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace paracuber {
class Communicator;
class Task;
class TaskResult;
class Log;

/** @brief Environment for running a \ref Task in.
 *
 * This class implements a thread pool of running worker threads, each
 * executing \ref Task objects.
 */
class Runner
{
  public:
  /** @brief Create a runner for tasks.
   *
   * This constructor does not start the internal thread pool yet. */
  Runner(Communicator* communicator, ConfigPtr config, LogPtr log);
  /** Destructor */
  ~Runner();

  /** @brief Start the thread-pool asynchronously.
   *
   * This function returns immediately. */
  void start();
  /** @brief Ends the thread-pool synchronously.
   *
   * This function returns once the last thread has finished. */
  void stop();

  /** @brief Reports if the runner is already running. */
  inline bool isRunning() { return m_running; }

  /** @brief Push a new task to the internal task queue.
   *
   * The task will be run as soon as priorities, dependencies, ..., are sorted
   * out. */
  std::future<std::unique_ptr<TaskResult>> push(std::unique_ptr<Task> task);

  private:
  friend class Communicator;

  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  Communicator* m_communicator;
  volatile bool m_running = true;

  std::vector<std::thread> m_pool;

  void worker(uint32_t workerId, Logger logger);

  struct QueueEntry
  {
    /** @brief Quick Constructor for a QueueEntry object.
     */
    QueueEntry(std::unique_ptr<Task> task, int priority);
    ~QueueEntry();
    std::unique_ptr<Task> task;
    std::promise<std::unique_ptr<TaskResult>> result;
    int priority = 0;

    inline bool operator<(QueueEntry const& b) const
    {
      return priority < b.priority;
    }
  };

  void push_(std::unique_ptr<QueueEntry> entry);
  std::unique_ptr<QueueEntry> pop_();
  std::vector<std::unique_ptr<QueueEntry>> m_taskQueue;

  std::mutex m_taskQueueMutex;
  std::condition_variable m_newTasks;

  /// Used against spurious wake-ups:
  /// https://en.cppreference.com/w/cpp/thread/condition_variable
  bool m_newTasksVerifier = false;

  std::vector<Task*> m_currentlyRunningTasks;
  std::atomic<uint32_t> m_numberOfRunningTasks;
};
}

#endif
