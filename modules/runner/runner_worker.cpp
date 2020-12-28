#include <cassert>

#include "paracooba/common/thread_registry.h"
#include "runner_worker.hpp"

#include "paracooba/common/log.h"
#include <paracooba/common/path.h>
#include <paracooba/common/task.h>
#include <paracooba/common/task_store.h>
#include <paracooba/module.h>

#include <distrac/distrac.h>
#include <distrac_paracooba.h>

namespace parac::runner {
Worker::Worker(parac_module& mod,
               parac_task_store& taskStore,
               std::mutex& notifierMutex,
               std::condition_variable& notifier,
               std::atomic_bool& notifierCheck,
               std::atomic_bool& stop,
               parac_worker workerId)
  : m_mod(mod)
  , m_taskStore(taskStore)
  , m_notifierMutex(notifierMutex)
  , m_notifier(notifier)
  , m_notifierCheck(notifierCheck)
  , m_stop(stop)
  , m_workerId(workerId)
  , m_threadRegistryHandle(std::make_unique<parac_thread_registry_handle>()) {
  threadRegistryHandle().userdata = this;
}
Worker::Worker(Worker&& o)
  : m_mod(o.m_mod)
  , m_taskStore(o.m_taskStore)
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

    int64_t startOfProcessing;
    if(m_mod.handle->distrac) {
      parac_ev_start_processing_task start_processing_task{
        m_currentTask->originator,
        { .rep = m_currentTask->path.rep },
        m_workerId
      };
      startOfProcessing = distrac_push(m_mod.handle->distrac,
                                       &start_processing_task,
                                       PARAC_EV_START_PROCESSING_TASK);
    }

    if(m_currentTask->work) {
      m_currentTask->result = m_currentTask->work(m_currentTask, m_workerId);
    }

    m_currentTask->state = static_cast<parac_task_state>(m_currentTask->state &
                                                         ~PARAC_TASK_WORKING) |
                           PARAC_TASK_DONE;

    if(m_mod.handle->distrac) {
      uint32_t diff =
        (distrac_current_time(m_mod.handle->distrac) - startOfProcessing) /
        (1000);
      parac_ev_finish_processing_task finish_processing_task{
        m_currentTask->originator,
        distrac_parac_path{ .rep = m_currentTask->path.rep },
        static_cast<uint16_t>(m_workerId),
        static_cast<uint16_t>(m_currentTask->result),
        diff
      };
      distrac_push(m_mod.handle->distrac,
                   &finish_processing_task,
                   PARAC_EV_FINISH_PROCESSING_TASK);
    }

    if(!(m_currentTask->state & PARAC_TASK_SPLITTED)) {
      m_taskStore.assess_task(&m_taskStore, m_currentTask);
    }
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

  if(m_mod.handle->distrac) {
    parac_ev_worker_idle worker_idle{ m_workerId };
    distrac_push(m_mod.handle->distrac, &worker_idle, PARAC_EV_WORKER_IDLE);
  }

  parac_log(
    PARAC_RUNNER, PARAC_TRACE, "Worker going into idle and waiting for tasks.");

  std::unique_lock lock(m_notifierMutex);
  while(!m_stop) {
    bool pop = false;
    if(!m_notifierCheck.exchange(false)) {
      pop = true;
    } else {
      m_notifier.wait(lock);
      if(m_notifierCheck.exchange(false) && !m_stop) {
        pop = true;
      }
    }

    if(pop) {
      // Only this thread has been woken up and catched the notifier! Return
      // next task.
      if(m_mod.handle->distrac) {
        parac_ev_worker_working worker_working{ m_workerId };
        distrac_push(
          m_mod.handle->distrac, &worker_working, PARAC_EV_WORKER_WORKING);
      }
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
