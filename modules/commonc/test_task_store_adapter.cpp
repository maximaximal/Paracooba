#include <catch2/catch.hpp>

#include <paracooba/common/task_store.h>
#include <paracooba/common/task_store_adapter.h>

#include <memory>

TEST_CASE("Create, Use and Destroy Task Store Adapter",
          "[commonc][task-store-adapter]") {
  parac_task_store_adapter adapter;
  parac_task_store_adapter_init(&adapter);

  REQUIRE(!adapter._target);

  parac_task_store store;
  parac_task_store& adapterStore = adapter.store;

  store.empty = [](struct parac_task_store* store) -> bool {
    (void)store;
    return true;
  };
  store.get_size = [](struct parac_task_store* store) -> size_t {
    (void)store;
    return 0;
  };
  store.push = [](struct parac_task_store* store,
                  const struct parac_task* task) {
    (void)store;
    (void)task;
  };
  store.pop_top = [](struct parac_task_store* store, struct parac_task* task) {
    (void)store;
    (void)task;
  };
  store.pop_bottom = [](struct parac_task_store* store,
                        struct parac_task* task) {
    (void)store;
    (void)task;
  };

  adapter._target = &store;

  REQUIRE(adapterStore.empty(&adapterStore));
  REQUIRE(adapterStore.get_size(&adapterStore) == 0);
}
