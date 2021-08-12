#include <cassert>

#include "paracooba/common/status.h"
#include "paracooba/common/thread_registry.h"
#include "runner_worker.hpp"

#include "paracooba/common/log.h"
#include <paracooba/common/path.h>
#include <paracooba/common/task.h>
#include <paracooba/common/task_store.h>
#include <paracooba/module.h>

#ifdef ENABLE_DISTRAC
#include <distrac/distrac.h>
#include <distrac/types.h>
#include <distrac_paracooba.h>
#endif

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
    m_idle = false;

    m_currentTask->worker = m_workerId;
    m_deleteNotifier = false;
    m_currentTask->delete_notification = &m_deleteNotifier;
    parac_id originator = m_currentTask->originator;
    parac_path p = m_currentTask->path;

    assert(!(m_currentTask->state & PARAC_TASK_WORK_AVAILABLE));

    parac_log(PARAC_RUNNER,
              PARAC_TRACE,
              "Starting work on path {}",
              m_currentTask->path);

    int64_t startOfProcessing;

#ifdef ENABLE_DISTRAC
    if(m_mod.handle->distrac) {
      distrac_parac_path dp;
      dp.rep = m_currentTask->path.rep;
      parac_ev_start_processing_task start_processing_task{
        m_currentTask->originator, dp, m_workerId
      };
      startOfProcessing = distrac_push(m_mod.handle->distrac,
                                       &start_processing_task,
                                       PARAC_EV_START_PROCESSING_TASK);
    }
#endif

    parac_status result = PARAC_ABORTED;
    if(m_currentTask->work) {
      result = m_currentTask->work(m_currentTask, m_workerId);
      if(result != PARAC_ABORTED && result != PARAC_PENDING &&
         !m_deleteNotifier && m_currentTask->result == PARAC_PENDING) {
        m_currentTask->result = result;
      }
    }

    if(result != PARAC_ABORTED && !m_deleteNotifier) {
      m_currentTask->state = static_cast<parac_task_state>(
                               m_currentTask->state & ~PARAC_TASK_WORKING) |
                             PARAC_TASK_DONE;
    }

#ifdef ENABLE_DISTRAC
    if(m_mod.handle->distrac) {
      uint32_t diff =
        (distrac_current_time(m_mod.handle->distrac) - startOfProcessing) /
        (1000);
      distrac_parac_path dp;
      dp.rep = p.rep;
      parac_ev_finish_processing_task finish_processing_task{
        originator,
        dp,
        static_cast<uint16_t>(m_workerId),
        static_cast<uint16_t>(result),
        diff
      };
      distrac_push(m_mod.handle->distrac,
                   &finish_processing_task,
                   PARAC_EV_FINISH_PROCESSING_TASK);
    }
#endif

    if(!m_deleteNotifier)
      m_currentTask->delete_notification = NULL;

    if(result != PARAC_ABORTED && !m_deleteNotifier &&
       !(m_currentTask->state & PARAC_TASK_SPLITTED)) {
      m_taskStore.assess_task(&m_taskStore, m_currentTask);
    }
  }
  parac_log(PARAC_RUNNER, PARAC_DEBUG, "Worker exited.");

  return PARAC_OK;
}

parac_task*
Worker::getNextTask() {
  auto work = m_taskStore.pop_work(&m_taskStore);
  if(work) {
    return work;
  }

  std::unique_lock lock(m_notifierMutex);

  while(!m_stop) {
    auto work = m_taskStore.pop_work(&m_taskStore);
    if(work) {
      return work;
    }

    if(!m_notifierCheck.exchange(false)) {
      if(!m_idle) {
        m_idle = true;
        parac_log(PARAC_RUNNER,
                  PARAC_TRACE,
                  "Worker going into idle and waiting for tasks.");
#ifdef ENABLE_DISTRAC
        if(m_mod.handle->distrac) {
          parac_ev_worker_idle worker_idle{ m_workerId };
          distrac_push(
            m_mod.handle->distrac, &worker_idle, PARAC_EV_WORKER_IDLE);
        }
#endif
      }
      if(m_stop)
        return nullptr;
      m_notifier.wait(lock);
    }
    work = m_taskStore.pop_work(&m_taskStore);
    if(work && !m_stop) {
      // Only this thread has been woken up and catched the notifier! Return
      // next task.
#ifdef ENABLE_DISTRAC
      if(m_mod.handle->distrac) {
        parac_ev_worker_working worker_working{ m_workerId };
        distrac_push(
          m_mod.handle->distrac, &worker_working, PARAC_EV_WORKER_WORKING);
      }
#endif
      return work;
    }
  }

  return nullptr;
}

struct parac_path
Worker::currentTaskPath() const {
  if(m_currentTask) {
    return m_currentTask->path;
  } else {
    parac_path p;
    p.rep = PARAC_PATH_EXPLICITLY_UNKNOWN;
    return p;
  }
}

parac_thread_registry_handle&
Worker::threadRegistryHandle() {
  return *m_threadRegistryHandle;
}

}
