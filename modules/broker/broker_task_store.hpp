#pragma once

#include "paracooba/common/types.h"
#include <functional>
#include <memory>

struct parac_handle;
struct parac_path;
struct parac_task;
struct parac_message;
struct parac_task_store;
struct parac_compute_node;
struct parac_timeout;

namespace parac::broker {
class TaskStore {
  public:
  explicit TaskStore(parac_handle& handle,
                     parac_task_store& store,
                     uint32_t autoShutdownTimeout);
  virtual ~TaskStore();

  using TaskChecker = std::function<bool(parac_task&)>;

  bool empty() const;
  size_t size() const;
  parac_task* newTask(parac_task* parent_task,
                      parac_path new_path,
                      parac_id originator);

  /** @brief Pop task for offloading. */
  parac_task* pop_offload(parac_compute_node* target,
                          TaskChecker check = nullptr);
  /** @brief Pop task for working. */
  parac_task* pop_work();

  void undo_offload(parac_task* t);

  void assess_task(parac_task* task);

  parac_task_store& store();

  void receiveTaskResultFromPeer(parac_message& msg);

  void manageAutoShutdownTimer();

  void terminateAllTasks();

  private:
  void insert_into_tasksWaitingForWorkerQueue(parac_task* task);
  void insert_into_tasksWaitingForChildren(parac_task* task);
  void insert_into_tasksBeingWorkedOn(parac_task* task);

  void remove_from_tasksWaitingForChildren(parac_task* task);
  void remove_from_tasksBeingWorkedOn(parac_task* task);
  void remove(parac_task* task);

  static void autoShutdownTimerExpired(parac_timeout* t);

  struct Internal;
  std::unique_ptr<Internal> m_internal;

  uint32_t m_autoShutdownTimeout;
};
}
