#pragma once

#include "paracooba/common/types.h"
#include <memory>

struct parac_handle;
struct parac_path;
struct parac_task;
struct parac_task_store;
struct parac_compute_node;

namespace parac::broker {
class TaskStore {
  public:
  explicit TaskStore(parac_handle& handle, parac_task_store& store);
  ~TaskStore();

  bool empty() const;
  size_t size() const;
  parac_task* newTask(parac_task* parent_task);

  /** @brief Pop task for offloading. */
  parac_task* pop_offload(parac_compute_node* target);
  /** @brief Pop task for working. */
  parac_task* pop_work();

  void assess_task(parac_task* task);

  void insert_into_tasksWaitingForWorkerQueue(parac_task* task);
  void insert_into_tasksWaitingForChildren(parac_task* task);
  void insert_into_tasksBeingWorkedOn(parac_task* task);

  void remove_from_tasksWaitingForChildren(parac_task* task);
  void remove_from_tasksBeingWorkedOn(parac_task* task);
  void remove(parac_task* task);

  struct Internal;
  std::unique_ptr<Internal> m_internal;
};
}
