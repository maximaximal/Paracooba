#include <catch2/catch.hpp>

#include <paracooba/common/thread_registry.h>

TEST_CASE("Starting and Waiting For Threads", "[commonc][thread_registry]") {
  parac_thread_registry registry;
  parac_thread_registry_init(&registry);

  parac_thread_registry_handle* handle = nullptr;

  parac_status status = parac_thread_registry_create(
    &registry,
    nullptr,
    [](parac_thread_registry_handle* handle) {
      REQUIRE(handle->running);
      REQUIRE(handle->thread_id == 1);
      REQUIRE(!handle->stop);
      return 10;
    },
    &handle);
  REQUIRE(status == PARAC_OK);

  parac_thread_registry_wait_for_exit(&registry);

  REQUIRE(handle->exit_status == 10);
  REQUIRE(!handle->running);

  parac_thread_registry_free(&registry);
}
