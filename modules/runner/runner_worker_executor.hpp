#pragma once

#include "paracooba/common/status.h"
#include <memory>

struct parac_task_store;
struct parac_module;
struct parac_config_entry;

namespace parac::runner {
class WorkerExecutor {
  public:
  enum Config { WORKER_COUNT, _CONFIG_COUNT };

  explicit WorkerExecutor(parac_module& mod, parac_config_entry* entries);
  ~WorkerExecutor();

  parac_status init();
  void exit();

  void workping(parac_task_store* store);

  uint32_t workerCount() const;

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;
};
}
