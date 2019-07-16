#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/task.hpp"
#include <boost/log/attributes/constant.hpp>

#include <algorithm>

namespace paracuber {
Runner::Runner(Communicator* communicator, ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger())
  , m_communicator(communicator)
  , m_numberOfRunningTasks(0)
{}
Runner::~Runner() {}

void
Runner::start()
{
  uint32_t i = 0, threadCount = m_config->getUint32(Config::ThreadCount);
  m_currentlyRunningTasks.assign(threadCount, nullptr);

  try {
    m_running = true;
    for(i = 0; i < threadCount; ++i) {
      m_pool.push_back(std::thread(
        std::bind(&Runner::worker, this, i, m_log->createLogger())));
    }
  } catch(std::system_error& e) {
    PARACUBER_LOG(m_logger, LocalError)
      << "Could only initialize " << i << " of " << threadCount
      << " requested threads!";
  }
}

void
Runner::stop()
{
  m_running = false;
  m_newTasks.notify_all();
  std::for_each(m_currentlyRunningTasks.begin(),
                m_currentlyRunningTasks.end(),
                [](auto& t) {
                  if(t)
                    t->terminate();
                });
  std::for_each(m_pool.begin(), m_pool.end(), [](auto& t) { t.join(); });
  m_pool.clear();
}

std::future<std::unique_ptr<TaskResult>>
Runner::push(std::unique_ptr<Task> task)
{
  task->m_runner = this;
  task->m_log = m_log.get();
  task->m_config = m_config.get();
  task->m_communicator = m_communicator;

  std::unique_ptr<QueueEntry> entry =
    std::make_unique<QueueEntry>(std::move(task), 0);
  std::promise<std::unique_ptr<TaskResult>>& promise = entry->result;
  push_(std::move(entry));
  std::unique_lock<std::mutex> lock(m_taskQueueMutex);
  m_newTasksVerifier = true;
  m_newTasks.notify_one();
  return promise.get_future();
}

void
Runner::worker(uint32_t workerId, Logger logger)
{
  logger.add_attribute(
    "workerID", boost::log::attributes::constant<decltype(workerId)>(workerId));
  PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " started.";
  while(m_running) {
    std::unique_ptr<QueueEntry> entry;
    {
      std::unique_lock<std::mutex> lock(m_taskQueueMutex);
      while(m_running && !m_newTasksVerifier) {
        PARACUBER_LOG(logger, Trace)
          << "Worker " << workerId << " waiting for tasks.";
        m_newTasks.wait(lock);
        entry = pop_();
      }

      if(entry) {
        // Only reset the verifier if this really was the thread to receive the
        // notification and this thread was not just started.
        m_newTasksVerifier = false;
      }
    }

    if(entry) {
      PARACUBER_LOG(logger, Trace)
        << "Worker " << workerId << " has received a new task.";

      // Insert the logger from this worker thread.
      entry->task->m_logger = &logger;

      m_currentlyRunningTasks[workerId] = entry->task.get();
      ++m_numberOfRunningTasks;
      auto result = std::move(entry->task->execute());
      --m_numberOfRunningTasks;
      m_currentlyRunningTasks[workerId] = nullptr;
      entry->task->finish(*result);

      entry->result.set_value(std::move(result));
    }
  }
  PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " ended.";
}

Runner::QueueEntry::QueueEntry(std::unique_ptr<Task> task, int priority)
  : task(std::move(task))
  , priority(priority)
{}
Runner::QueueEntry::~QueueEntry() {}

void
Runner::push_(std::unique_ptr<QueueEntry> entry)
{
  m_taskQueue.push_back(std::move(entry));
  std::push_heap(m_taskQueue.begin(), m_taskQueue.end());
}
std::unique_ptr<Runner::QueueEntry>
Runner::pop_()
{
  if(m_taskQueue.size() > 0) {
    std::pop_heap(m_taskQueue.begin(), m_taskQueue.end());
    auto result = std::move(m_taskQueue.back());
    m_taskQueue.pop_back();
    return result;
  }
  return nullptr;
}
}
