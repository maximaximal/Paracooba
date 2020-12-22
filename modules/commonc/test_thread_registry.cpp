#include <catch2/catch.hpp>
#include <mutex>

#include <paracooba/common/thread_registry.h>

TEST_CASE("Starting and Waiting For Threads", "[commonc][thread_registry]") {
  paracooba::ThreadRegistryWrapper registry(0);

  parac_thread_registry_handle handle;

  static bool passed = false;

  parac_thread_registry_add_starting_callback(
    &registry, [](parac_thread_registry_handle* handle) {
      passed = !handle->running && handle->thread_id == 1;
    });

  REQUIRE(!passed);

  parac_status status = parac_thread_registry_create(
    &registry,
    nullptr,
    [](parac_thread_registry_handle* handle) {
      return handle->running && handle->thread_id == 1 ? 10 : 11;
    },
    &handle);
  REQUIRE(status == PARAC_OK);

  REQUIRE(!handle.stop);
  parac_thread_registry_stop(&registry);
  REQUIRE(handle.stop);

  parac_thread_registry_wait_for_exit(&registry);
  REQUIRE(passed);

  REQUIRE(handle.exit_status == 10);
  REQUIRE(!handle.running);
}
