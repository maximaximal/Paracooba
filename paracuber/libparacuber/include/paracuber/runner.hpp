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
  Runner(ConfigPtr config, LogPtr log);
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

  /** @brief Push a new task to the internal task queue.
   *
   * The task will be run as soon as priorities, dependencies, ..., are sorted
   * out. */
  std::future<std::unique_ptr<TaskResult>>& push(std::unique_ptr<Task> task);

  private:
  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
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
    std::future<std::unique_ptr<TaskResult>> result;
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
};
}

#endif
