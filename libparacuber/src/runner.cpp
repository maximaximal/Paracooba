#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/task.hpp"
#include "../include/paracuber/task_factory.hpp"
#include <boost/log/attributes/constant.hpp>

#include <algorithm>
#include <chrono>

namespace paracuber {
Runner::Runner(Communicator* communicator, ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger("Runner"))
  , m_communicator(communicator)
  , m_numberOfRunningTasks(0)
  , m_taskQueue(
      std::make_unique<PriorityQueueLockSemanticsUniquePtr<QueueEntry>>())
  , m_autoShutdownTimer(communicator->getIOService())
{}
Runner::~Runner()
{
  stop();
  PARACUBER_LOG(m_logger, Trace) << "Destruct Runner.";
}

void
Runner::start()
{
  uint32_t i = 0, threadCount = m_config->getUint32(Config::ThreadCount);
  m_currentlyRunningTasks.assign(threadCount, nullptr);

  PARACUBER_LOG(m_logger, Trace) << "Starting Runner";

  try {
    m_running = true;
    for(i = 0; i < threadCount; ++i) {
      m_pool.push_back(std::thread(std::bind(&Runner::worker, this, i)));
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
  PARACUBER_LOG(m_logger, Trace) << "Stopping Runner";

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
Runner::push(std::unique_ptr<Task> task,
             int64_t originator,
             int priority,
             TaskFactory* factory)
{
  task->m_runner = this;
  task->m_log = m_log.get();
  task->m_config = m_config.get();
  task->m_communicator = m_communicator;

  std::unique_ptr<QueueEntry> entry = std::make_unique<QueueEntry>(
    std::move(task), priority, originator, factory);
  std::promise<std::unique_ptr<TaskResult>>& promise = entry->result;
  auto future = promise.get_future();
  std::unique_lock<std::mutex> lock(m_taskQueue->getMutex());
  m_taskQueue->pushNoLock(std::move(entry));
  m_newTasksVerifier = true;
  m_newTasks.notify_one();
  return std::move(future);
}

void
Runner::registerTaskFactory(TaskFactory* f)
{
  f->initWorkerSlots(m_config->getUint32(Config::ThreadCount));
  std::unique_lock lock(m_taskFactoriesMutex);
  m_taskFactories.push_back(f);
}
void
Runner::deregisterTaskFactory(TaskFactory* f)
{
  // To deregister, the factory is first removed from the ones producing tasks.
  // Then all already produced tasks are removed. Afterwards, already running
  // tasks are terminated.

  {
    std::unique_lock lock(m_taskFactoriesMutex);
    m_taskFactories.erase(
      std::find(m_taskFactories.begin(), m_taskFactories.end(), f));
  }

  m_taskQueue->removeMatching(
    [f](const auto& e) { return e && e->factory == f; });

  for(Task* task : m_currentlyRunningTasks) {
    if(task) {
      if(task->getTaskFactory() == f) {
        task->terminate();
      }
    }
  }
}

void
Runner::worker(uint32_t workerId)
{
  auto logger = m_log->createLogger("Worker", std::to_string(workerId));
  PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " started.";
  while(m_running) {
    std::unique_ptr<QueueEntry> entry = nullptr;
    {
      checkTaskFactories();
      conditionallySetAutoShutdownTimer();
      std::unique_lock<std::mutex> lock(m_taskQueue->getMutex());
      while(m_running && !m_newTasksVerifier && m_taskQueue->size() == 0) {
        PARACUBER_LOG(logger, Trace)
          << "Worker " << workerId << " waiting for tasks.";
        m_newTasks.wait(lock);
      }

      if(m_taskQueue->size() > 0) {
        entry = m_taskQueue->popNoLock();
        m_newTasksVerifier = false;
      }
    }

    if(entry) {
      resetAutoShutdownTimer();

      if(entry->task) {
        PARACUBER_LOG(logger, Trace)
          << "Worker " << workerId
          << " has received a new task: " << entry->task->name()
          << " with priority " << entry->priority;

        // Insert the logger from this worker thread.
        entry->task->m_logger = &logger;
        entry->task->m_factory = entry->factory;
        entry->task->m_originator = entry->originator;
        entry->task->m_workerId = workerId;

        m_currentlyRunningTasks[workerId] = entry->task.get();
        ++m_numberOfRunningTasks;
        auto result = std::move(entry->task->execute());
        --m_numberOfRunningTasks;
        m_currentlyRunningTasks[workerId] = nullptr;
        result->setTask(std::move(entry->task));
        result->getTask().finish(*result);
        entry->result.set_value(std::move(result));
      } else {
        PARACUBER_LOG(logger, LocalError)
          << "Worker " << workerId
          << " received a task queue item without a valid task!";
      }
    }
  }
  PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " ended.";
}

Runner::QueueEntry::QueueEntry(std::unique_ptr<Task> task,
                               int priority,
                               int64_t originator,
                               TaskFactory* factory)
  : task(std::move(task))
  , priority(priority)
  , originator(originator)
  , factory(factory)
{}
Runner::QueueEntry::~QueueEntry() {}

void
Runner::checkTaskFactories()
{
  static int i = 0;

  std::shared_lock lock(m_taskFactoriesMutex);
  for(auto factory : m_taskFactories) {
    // Check if more tasks should be requested for every factory and if new
    // tasks should be requested, request multiple of them.
    while(m_taskQueue->size() < m_currentlyRunningTasks.size() * 3 &&
          factory->canProduceTask()) {
      // Receive and submit the task.
      auto [task, originator, priority] = factory->produceTaskBackwards();

      if(task) {
        push(std::move(task), originator, priority, factory);
      }
    }
  }
}
void
Runner::conditionallySetAutoShutdownTimer()
{
  int32_t seconds = m_config->getInt32(Config::AutoShutdown);
  if(seconds < 0 || m_numberOfRunningTasks > 0 || m_autoShutdownArmed ||
     m_taskQueue->size() > 0)
    return;

  std::unique_lock lock(m_autoShutdownTimerMutex);
  if(m_autoShutdownArmed)
    return;

  m_autoShutdownArmed = true;

  PARACUBER_LOG(m_logger, LocalWarning) << "Starting automatic shutdown timer!";

  m_autoShutdownTimer.expires_after(std::chrono::seconds(seconds));

  m_autoShutdownTimer.async_wait([this](const boost::system::error_code& errc) {
    if(errc != boost::asio::error::operation_aborted)
      m_communicator->exit();
  });
}
void
Runner::resetAutoShutdownTimer()
{
  if(!m_autoShutdownArmed)
    return;

  std::unique_lock lock(m_autoShutdownTimerMutex);

  m_autoShutdownArmed = false;

  PARACUBER_LOG(m_logger, LocalWarning) << "Auto-Shutdown Canceled.";
  m_autoShutdownTimer.cancel();
}
}
