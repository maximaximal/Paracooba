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
  REQUIRE(store->pop_work(store) == nullptr);

  task->state = PARAC_TASK_WORK_AVAILABLE;
  store->assess_task(store, task);

  REQUIRE(store->pop_work(store) == task);
  task->state =
    static_cast<parac_task_state>(task->state & ~PARAC_TASK_WORKING);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->pop_work(store) == nullptr);

  task->result = PARAC_PENDING;
  task->right_result = PARAC_UNSAT;
  task->state = task->state | PARAC_TASK_DONE | PARAC_TASK_WAITING_FOR_SPLITS |
                PARAC_TASK_SPLITTED;

  auto leftp = parac_path_get_next_left(task->path);
  parac_task* task2 = store->new_task(store, task, leftp, 0);

  REQUIRE(task2);
  REQUIRE(task2->parent_task_ == task);
  REQUIRE(task2->parent_task_->left_child_ == task2);

  REQUIRE(store->get_size(store) == 2);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);

  task2->state = PARAC_TASK_WORK_AVAILABLE;
  store->assess_task(store, task2);

  REQUIRE(store->get_waiting_for_worker_size(store) == 1);

  task2->serialize = [](struct parac_task*, struct parac_message*) {
    return PARAC_OK;
  };
  REQUIRE(store->pop_offload(store, this_node) == task2);
  task2->serialize = nullptr;

  REQUIRE(store->get_waiting_for_children_size(store) == 0);
  CAPTURE(task->state);
  store->assess_task(store, task);
  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);
  REQUIRE(store->get_waiting_for_children_size(store) == 1);

  task2->result = PARAC_OK;
  task2->state = PARAC_TASK_DONE | PARAC_TASK_SPLITS_DONE;
  CAPTURE(task2->state);
  store->assess_task(store, task2);

  REQUIRE(store->get_waiting_for_children_size(store) == 0);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);
  REQUIRE(store->get_tasks_being_worked_on_size(store) == 0);
}
