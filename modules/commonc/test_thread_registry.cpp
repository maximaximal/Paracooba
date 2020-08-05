#include <catch2/catch.hpp>

#include <paracooba/common/thread_registry.h>

TEST_CASE("Starting and Waiting For Threads", "[commonc][thread_registry]") {
  parac_thread_registry registry;
  parac_thread_registry_init(&registry);

  parac_thread_registry_free(&registry);
}
