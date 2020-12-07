// Deactivate leak checks, as boost memory pool is acting up when unit testing.
extern "C" const char*
__asan_default_options() {
  return "detect_leaks=0";
}

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
