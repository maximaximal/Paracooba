#include <catch2/catch.hpp>

#include <paracooba/common/task.h>
#include <paracooba/common/task_store.h>

#include "paracooba/common/compute_node_store.h"
#include "paracooba/common/path.h"

#include "mocks.hpp"
#include "paracooba/common/status.h"
#include "paracooba/module.h"

TEST_CASE("Test Task Store: Manipulating Tasks",
          "[integration,broker,taskstore]") {
  ParacoobaMock master(1, nullptr, nullptr, { PARAC_MOD_BROKER });

  parac_compute_node* this_node =
    master.getBroker().compute_node_store->this_node;
  REQUIRE(this_node);

  auto& broker = master.getBroker();
  auto store = broker.task_store;

  REQUIRE(store->empty);
  REQUIRE(store->pop_offload);
  REQUIRE(store->get_size);
  REQUIRE(store->new_task);
  REQUIRE(store->pop_work);

  REQUIRE(store->empty(store));
  REQUIRE(store->get_size(store) == 0);

  auto p = parac::Path::build(0b1000000, 1);

  parac_task* task = store->new_task(store, nullptr);
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

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->pop_work(store) == nullptr);

  task->result = PARAC_PENDING;
  task->state = task->state | PARAC_TASK_DONE | PARAC_TASK_WAITING_FOR_LEFT;

  parac_task* task2 = store->new_task(store, task);
  task2->path = parac_path_get_next_left(task->path);

  REQUIRE(task2);
  REQUIRE(task2->parent_task == task);

  REQUIRE(store->get_size(store) == 2);

  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);

  task2->state = PARAC_TASK_WORK_AVAILABLE;
  store->assess_task(store, task2);

  REQUIRE(store->get_waiting_for_worker_size(store) == 1);

  REQUIRE(store->pop_offload(store, this_node) == task2);

  REQUIRE(store->get_waiting_for_children_size(store) == 0);
  store->assess_task(store, task);
  REQUIRE(store->pop_offload(store, this_node) == nullptr);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);
  REQUIRE(store->get_waiting_for_children_size(store) == 1);

  task2->result = PARAC_OK;
  task2->state = PARAC_TASK_ALL_DONE;
  store->assess_task(store, task2);

  CAPTURE(task->state);
  CAPTURE(task2->state);

  REQUIRE(store->get_waiting_for_children_size(store) == 0);
  REQUIRE(store->get_waiting_for_worker_size(store) == 0);
  REQUIRE(store->get_tasks_being_worked_on_size(store) == 0);
}
