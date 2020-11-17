#include <catch2/catch.hpp>

#include <paracooba/common/task.h>
#include <paracooba/common/task_store.h>

#include "broker_task_store.hpp"
#include "paracooba/common/path.h"
#include "paracooba/module.h"

using namespace parac;
using namespace parac::broker;

TEST_CASE("Test Broker Task Store: Basics", "[broker][taskstore]") {
  parac_handle handle;
  parac_task_store p_store;
  TaskStore store(handle, p_store);

  REQUIRE(p_store.empty);
  REQUIRE(p_store.pop_offload);
  REQUIRE(p_store.get_size);
  REQUIRE(p_store.new_task);
  REQUIRE(p_store.pop_work);

  REQUIRE(p_store.empty(&p_store) == store.empty());
  REQUIRE(store.empty());
}
