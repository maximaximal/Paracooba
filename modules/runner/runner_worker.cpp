#include <cassert>

#include "paracooba/common/thread_registry.h"
#include "runner_worker.hpp"

#include "paracooba/common/log.h"
#include <paracooba/common/path.h>
#include <paracooba/common/task.h>
#include <paracooba/common/task_store.h>

namespace parac::runner {
Worker::Worker(parac_task_store& taskStore,
               std::mutex& notifierMutex,
               std::condition_variable& notifier,
               std::atomic_bool& notifierCheck,
               std::atomic_bool& stop,
               parac_worker workerId)
  : m_taskStore(taskStore)
  , m_notifierMutex(notifierMutex)
  , m_notifier(notifier)
  , m_notifierCheck(notifierCheck)
  , m_stop(stop)
  , m_workerId(workerId)
  , m_threadRegistryHandle(std::make_unique<parac_thread_registry_handle>()) {
  threadRegistryHandle().userdata = this;
}
Worker::Worker(Worker&& o)
  : m_taskStore(o.m_taskStore)
  , m_notifierMutex(o.m_notifierMutex)
  , m_notifier(o.m_notifier)
  , m_notifierCheck(o.m_notifierCheck)
  , m_stop(o.m_stop)
  , m_threadRegistryHandle(std::move(o.m_threadRegistryHandle)) {
  threadRegistryHandle().userdata = this;
}
Worker::~Worker() {}

parac_status
Worker::run() {
  parac_log(PARAC_RUNNER,
            PARAC_DEBUG,
            "Worker started (Runner-module-internal worker id: {})",
            m_workerId);
  while(!m_stop) {
    if(!(m_currentTask = getNextTask())) {
      continue;
    }
    assert(m_currentTask);

    assert(!(m_currentTask->state & PARAC_TASK_WORK_AVAILABLE));

    parac_log(PARAC_RUNNER,
              PARAC_TRACE,
              "Starting work on path {}",
              m_currentTask->path);

    if(m_currentTask->work) {
      m_currentTask->result = m_currentTask->work(m_currentTask, m_workerId);
    }

    m_currentTask->state = m_currentTask->state | PARAC_TASK_DONE;

    m_taskStore.assess_task(&m_taskStore, m_currentTask);
  }
  parac_log(PARAC_RUNNER, PARAC_DEBUG, "Worker exited.");

  return PARAC_OK;
}

parac_task*
Worker::getNextTask() {
  // New task is available and no other task has cleared the flag yet!
  // Immediately get next task and don't even lock the mutex.
  auto work = m_taskStore.pop_work(&m_taskStore);
  if(work) {
    m_notifierCheck.store(false);
    return work;
  }

  parac_log(
    PARAC_RUNNER, PARAC_TRACE, "Worker going into idle and waiting for tasks.");

  std::unique_lock lock(m_notifierMutex);
  while(!m_stop) {
    m_notifier.wait(lock);
    if(m_notifierCheck.exchange(false) && !m_stop) {
      // Only this thread has been woken up and catched the notifier! Return
      // next task.
      return m_taskStore.pop_work(&m_taskStore);
    }
  }

  return nullptr;
}

struct parac_path
Worker::currentTaskPath() const {
  if(m_currentTask) {
    return m_currentTask->path;
  } else {
    return parac_path{ .rep = PARAC_PATH_EXPLICITLY_UNKNOWN };
  }
}

parac_thread_registry_handle&
Worker::threadRegistryHandle() {
  return *m_threadRegistryHandle;
}

}
