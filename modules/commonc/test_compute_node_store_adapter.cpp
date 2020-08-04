#include <catch2/catch.hpp>

#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/compute_node_store_adapter.h>

#include <memory>

TEST_CASE("Create, Use and Destroy Compute Node Store Adapter",
          "[commonc][compute-node-store-adapter]") {
  parac_compute_node_store_adapter adapter;
  parac_compute_node_store_adapter_init(&adapter);

  REQUIRE(!adapter._target);

  parac_compute_node_store store;
  parac_compute_node_store& adapterStore = adapter.store;

  store.has = [](struct parac_compute_node_store* store, parac_id) -> bool {
    (void)store;
    return false;
  };
  store.get = [](struct parac_compute_node_store * store,
                 parac_id) -> struct parac_compute_node* {
    (void)store;
    return nullptr;
  };

  adapter._target = &store;

  REQUIRE(!adapterStore.has(&adapterStore, 0));
  REQUIRE(adapterStore.get(&adapterStore, 0) == nullptr);
}
