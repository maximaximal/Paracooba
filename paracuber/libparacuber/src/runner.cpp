#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/task.hpp"

#include <algorithm>

namespace paracuber {
Runner::Runner(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger())
{}
Runner::~Runner() {}

void
Runner::start()
{
  m_running = true;
  for(uint32_t i = 0; i < m_config->getUint32(Config::ThreadCount); ++i) {
    m_pool.push_back(
      std::thread(std::bind(&Runner::worker, this, i, m_log->createLogger())));
  }
}

void
Runner::stop()
{
  m_running = false;
  m_newTasks.notify_all();
  std::for_each(m_pool.begin(), m_pool.end(), [](auto& t) { t.join(); });
}

std::future<std::unique_ptr<TaskResult>>&
Runner::push(std::unique_ptr<Task> task)
{
  std::unique_ptr<QueueEntry> entry =
    std::make_unique<QueueEntry>(std::move(task), 0);
  std::future<std::unique_ptr<TaskResult>>& future = entry->result;
  push_(std::move(entry));
  m_newTasksVerifier = true;
  m_newTasks.notify_one();
  return future;
}

void
Runner::worker(uint32_t workerId, Logger logger)
{
  PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " started.";
  while(m_running) {
    std::unique_ptr<QueueEntry> entry;
    {
      std::unique_lock<std::mutex> lock(m_taskQueueMutex);
      while(m_running && !m_newTasksVerifier) {
        PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " waiting for tasks.";
        m_newTasks.wait(lock);
        m_newTasksVerifier = false;
        entry = pop_();
      }
    }

    if(entry) {
      PARACUBER_LOG(logger, Trace)
        << "Worker " << workerId << " has received new task.";
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
  std::unique_lock<std::mutex> lock(m_taskQueueMutex);
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
