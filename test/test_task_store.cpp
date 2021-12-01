#include <catch2/catch.hpp>

#include <paracooba/common/task.h>
#include <paracooba/common/task_store.h>

#include "paracooba/common/compute_node_store.h"
#include "paracooba/common/path.h"

#include "mocks.hpp"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/module.h"

TEST_CASE("Test Task Store: Manipulating Tasks",
          "[integration,broker,taskstore]") {
  ParacoobaMock master(1, nullptr, nullptr, { PARAC_MOD_BROKER });

  auto& broker = master.getBroker();
  auto compNodeStore = broker.compute_node_store;
  REQUIRE(compNodeStore);

  parac_compute_node* this_node = compNodeStore->this_node;
  REQUIRE(this_node);

  auto store = broker.task_store;

  REQUIRE(store->empty);
  REQUIRE(store->pop_offload);
  REQUIRE(store->get_size);
  REQUIRE(store->new_task);
  REQUIRE(store->pop_work);

  REQUIRE(store->empty(store));
  REQUIRE(store->get_size(store) == 0);

  auto p = parac::Path::build(0b1000000, 1);

  parac_task* task = store->new_task(store, nullptr, parac_path_unknown(), 0);
  REQUIRE(task);
  REQUIRE(task->left_result == PARAC_PENDING);
  REQUIRE(task->right_result == PARAC_PENDING);

  task->path = p;

  REQUIRE(!store->empty(store));
  REQUIRE(store->get_size(store) == 1);

  REQUIRE(task->state == PARAC_TASK_NEW);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->pop_work(store, nullptr) == nullptr);

  task->state = PARAC_TASK_WORK_AVAILABLE;
  store->assess_task(store, task);

  REQUIRE(store->pop_work(store, nullptr) == task);
  task->state =
    static_cast<parac_task_state>(task->state & ~PARAC_TASK_WORKING);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->pop_work(store, nullptr) == nullptr);

  task->result = PARAC_PENDING;
  task->state = task->state | PARAC_TASK_DONE | PARAC_TASK_WAITING_FOR_SPLITS |
                PARAC_TASK_SPLITTED;

  auto leftp = parac_path_get_next_left(task->path);
  parac_task* task2 = store->new_task(store, task, leftp, 0);

  REQUIRE(task2);
  REQUIRE(task2->parent_task_ == task);
  REQUIRE(task2->parent_task_->left_child_ == task2);

  auto rightp = parac_path_get_next_right(task->path);
  parac_task* task3 = store->new_task(store, task, rightp, 0);

  REQUIRE(task3);
  REQUIRE(task3->parent_task_ == task);
  REQUIRE(task3->parent_task_->right_child_ == task3);

  REQUIRE(store->get_size(store) == 3);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);

  task2->state = PARAC_TASK_WORK_AVAILABLE;
  store->assess_task(store, task2);

  REQUIRE(store->get_waiting_for_worker_size(store) == 1);

  task3->state = PARAC_TASK_WORK_AVAILABLE;
  store->assess_task(store, task3);

  REQUIRE(store->get_waiting_for_worker_size(store) == 2);

  task2->serialize = [](struct parac_task*, struct parac_message*) {
    return PARAC_OK;
  };
  REQUIRE(store->pop_offload(store, this_node) == task2);

  task3->serialize = task2->serialize;
  task2->serialize = nullptr;

  REQUIRE(store->pop_offload(store, this_node) == task3);

  {
    REQUIRE(store->get_waiting_for_children_size(store) == 0);
    CAPTURE(task->state);
    store->assess_task(store, task);
    REQUIRE(store->pop_offload(store, this_node) == nullptr);
    REQUIRE(store->get_waiting_for_worker_size(store) == 0);
    REQUIRE(store->get_waiting_for_children_size(store) == 1);

    task2->result = PARAC_SAT;
    task2->state = PARAC_TASK_DONE | PARAC_TASK_SPLITS_DONE;
    task3->result = PARAC_SAT;
    task3->state = PARAC_TASK_DONE | PARAC_TASK_SPLITS_DONE;
  }

  {
    store->assess_task(store, task2);
    CAPTURE(task->state);
    CAPTURE(task2->state);
    CAPTURE(task->result);
    CAPTURE(task2->result);

    REQUIRE(store->get_waiting_for_children_size(store) == 0);
    REQUIRE(store->get_waiting_for_worker_size(store) == 0);
    REQUIRE(store->get_tasks_being_worked_on_size(store) == 0);
  }
}

TEST_CASE("Test Task Store: Manipulating Extended Tasks",
          "[integration,broker,taskstore]") {
  ParacoobaMock master(1, nullptr, nullptr, { PARAC_MOD_BROKER });

  auto& broker = master.getBroker();
  auto compNodeStore = broker.compute_node_store;
  REQUIRE(compNodeStore);

  parac_compute_node* this_node = compNodeStore->this_node;
  REQUIRE(this_node);

  auto store = broker.task_store;

  REQUIRE(store->empty);
  REQUIRE(store->pop_offload);
  REQUIRE(store->get_size);
  REQUIRE(store->new_task);
  REQUIRE(store->pop_work);

  REQUIRE(store->empty(store));
  REQUIRE(store->get_size(store) == 0);

  auto p = parac::Path::build(0b1000000, 1);

  parac_task* task = store->new_task(store, nullptr, parac_path_unknown(), 0);
  REQUIRE(task);
  REQUIRE(task->left_result == PARAC_PENDING);
  REQUIRE(task->right_result == PARAC_PENDING);

  task->path = p;

  REQUIRE(!store->empty(store));
  REQUIRE(store->get_size(store) == 1);

  REQUIRE(task->state == PARAC_TASK_NEW);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->pop_work(store, nullptr) == nullptr);

  task->state = PARAC_TASK_WORK_AVAILABLE;
  store->assess_task(store, task);

  REQUIRE(store->pop_work(store, nullptr) == task);
  task->state =
    static_cast<parac_task_state>(task->state & ~PARAC_TASK_WORKING);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->pop_work(store, nullptr) == nullptr);

  task->result = PARAC_PENDING;
  task->state = task->state | PARAC_TASK_DONE | PARAC_TASK_WAITING_FOR_SPLITS |
                PARAC_TASK_SPLITTED;

  // Create three child tasks, every one is required to complete the parent!

  auto extp = parac_path_get_next_extended(task->path);
  parac_task* subtasks[3] = { store->new_task(store, task, extp, 0),
                              store->new_task(store, task, extp, 0),
                              store->new_task(store, task, extp, 0) };
  parac_status subtasks_results[] = { PARAC_PENDING,
                                      PARAC_PENDING,
                                      PARAC_PENDING };
  task->extended_children = subtasks;
  task->extended_children_results = subtasks_results;
  task->extended_children_count = 3;
  task->assess = &parac_task_qbf_universal_assess;

  for(parac_task* subtask : subtasks) {
    REQUIRE(subtask);
    REQUIRE(subtask->parent_task_ == task);
  }

  REQUIRE(store->get_size(store) == 4);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);

  for(parac_task* subtask : subtasks) {
    subtask->state = PARAC_TASK_WORK_AVAILABLE;
    store->assess_task(store, subtask);

    subtask->serialize = [](struct parac_task*, struct parac_message*) {
      return PARAC_OK;
    };
  }

  REQUIRE(store->get_waiting_for_worker_size(store) == 3);

  // Now, offload all so that results may be set directly.
  for(parac_task* subtask : subtasks) {
    REQUIRE(store->pop_offload(store, this_node) == subtask);
  }

  {
    store->assess_task(store, task);

    CAPTURE(task->path);
    CAPTURE(task->result);
    CAPTURE(task->state);

    CAPTURE(subtasks[0]->path);
    CAPTURE(subtasks[0]->result);
    CAPTURE(subtasks[0]->state);

    REQUIRE(store->pop_offload(store, this_node) == nullptr);
    REQUIRE(store->get_waiting_for_worker_size(store) == 0);
    REQUIRE(store->get_waiting_for_children_size(store) == 1);
  }

  for(parac_task* subtask : subtasks) {
    subtask->result = PARAC_SAT;
    subtask->state = PARAC_TASK_DONE | PARAC_TASK_SPLITS_DONE;
  }
  for(parac_status& s : subtasks_results) {
    s = PARAC_SAT;
  }

  store->assess_task(store, task);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);
  REQUIRE(store->get_waiting_for_children_size(store) == 0);
}
