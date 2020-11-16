#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <memory>

#include <paracooba/common/status.h>

struct parac_path;
struct parac_task;
struct parac_task_store;
struct parac_thread_registry_handle;

namespace parac::runner {
class Worker {
  public:
  Worker(parac_task_store& taskStore,
         std::mutex& notifierMutex,
         std::condition_variable& notifier,
         std::atomic_bool& notifierCheck,
         std::atomic_bool& stop);
  Worker(const Worker& o) = delete;
  Worker(Worker&& o);
  ~Worker();

  parac_status run();

  bool working() const { return m_working; };
  parac_path currentTaskPath() const;

  parac_thread_registry_handle& threadRegistryHandle();

  private:
  parac_task_store& m_taskStore;
  std::mutex& m_notifierMutex;
  std::condition_variable& m_notifier;
  std::atomic_bool& m_notifierCheck;
  std::atomic_bool& m_stop;

  std::atomic_bool m_working;
  std::unique_ptr<parac_thread_registry_handle> m_threadRegistryHandle;

  parac_task* m_currentTask = nullptr;

  parac_task* getNextTask();
};
}
