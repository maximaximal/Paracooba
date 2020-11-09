#include <catch2/catch.hpp>

#include <paracooba/common/task.h>
#include <paracooba/common/task_store.h>

#include "broker_task_store.hpp"

using namespace parac;
using namespace parac::broker;

TEST_CASE("Test Broker Task Store: Basics", "[broker][taskstore]") {
  parac_task_store p_store;
  TaskStore store(p_store);

  REQUIRE(p_store.empty);
  REQUIRE(p_store.pop_top);
  REQUIRE(p_store.get_size);
  REQUIRE(p_store.new_task);
  REQUIRE(p_store.pop_bottom);

  REQUIRE(p_store.empty(&p_store) == store.empty());
  REQUIRE(store.empty());
}

TEST_CASE("Test Broker Task Store: Manipulating Tasks", "[broker][taskstore]") {
}
