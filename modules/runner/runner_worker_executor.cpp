#include "runner_worker_executor.hpp"
#include "paracooba/common/types.h"
#include "runner_worker.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <vector>

#include <paracooba/broker/broker.h>
#include <paracooba/common/config.h>
#include <paracooba/common/task_store.h>
#include <paracooba/common/thread_registry.h>
#include <paracooba/module.h>

namespace parac::runner {
struct WorkerExecutor::Internal {
  Internal(parac_module& mod, parac_config_entry* config_entries)
    : mod(mod)
    , config_entries(config_entries) {}

  parac_module& mod;
  parac_config_entry* config_entries;

  std::mutex notifierMutex;
  std::condition_variable notifier;
  std::atomic_bool notifierCheck = false;
  std::atomic_bool stop = false;

  std::vector<std::unique_ptr<Worker>> workers;
};

WorkerExecutor::WorkerExecutor(parac_module& mod,
                               parac_config_entry* config_entries)
  : m_internal(std::make_unique<Internal>(mod, config_entries)) {}
WorkerExecutor::~WorkerExecutor() {}
parac_status
WorkerExecutor::init() {
  assert(m_internal->mod.handle);
  assert(m_internal->mod.handle->modules[PARAC_MOD_BROKER]);
  assert(m_internal->mod.handle->modules[PARAC_MOD_BROKER]->broker);
  assert(m_internal->mod.handle->modules[PARAC_MOD_BROKER]->broker->task_store);

  auto& handle = *m_internal->mod.handle;
  parac_task_store& task_store =
    *handle.modules[PARAC_MOD_BROKER]->broker->task_store;

  parac_worker id = 0;
  while(m_internal->workers.size() < workerCount()) {
    m_internal->workers.emplace_back(
      std::make_unique<Worker>(task_store,
                               m_internal->notifierMutex,
                               m_internal->notifier,
                               m_internal->notifierCheck,
                               m_internal->stop, id++));

    auto& worker = m_internal->workers[m_internal->workers.size() - 1];

    parac_thread_registry_create(
      handle.thread_registry,
      handle.modules[PARAC_MOD_RUNNER],
      [](parac_thread_registry_handle* handle) -> int {
        Worker* worker = static_cast<Worker*>(handle->userdata);
        return worker->run();
      },
      &worker->threadRegistryHandle());
  }

  task_store.ping_on_work_userdata = this;
  task_store.ping_on_work = [](parac_task_store* store) {
    WorkerExecutor* ex =
      static_cast<WorkerExecutor*>(store->ping_on_work_userdata);
    ex->workping(store);
  };

  return PARAC_OK;
}

void
WorkerExecutor::exit() {
  m_internal->stop.store(true);
  m_internal->notifier.notify_all();
}

void
WorkerExecutor::workping(parac_task_store* store) {
  (void)store;
  m_internal->notifierCheck.store(true);
  m_internal->notifier.notify_one();
}

uint32_t
WorkerExecutor::workerCount() const {
  return m_internal->config_entries[WORKER_COUNT].value.uint32;
}
}
