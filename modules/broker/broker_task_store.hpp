#pragma once

#include "paracooba/common/types.h"
#include <memory>

struct parac_path;
struct parac_task;
struct parac_task_store;

namespace parac::broker {
class TaskStore {
  public:
  explicit TaskStore(parac_task_store& store);
  ~TaskStore();

  bool empty() const;
  size_t size() const;
  parac_task* newTask(parac_path path, parac_id originator);

  /** @brief Pop task for offloading. */
  parac_task* pop_top();
  /** @brief Pop task for working. */
  parac_task* pop_bottom();

  struct Internal;
  std::unique_ptr<Internal> m_internal;
};
}
