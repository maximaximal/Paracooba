#include <catch2/catch.hpp>

#include <paracooba/common/config.h>

TEST_CASE("Config Initialisation, Reserving, Registration and Free",
          "[commonc][config]") {
  paracooba::ConfigWrapper config;

  std::size_t entryCount = 2;

  parac_config_entry* entries = nullptr;

  entries = parac_config_reserve(&config, entryCount);
  REQUIRE(entries);
  REQUIRE(config.size == 2);
  REQUIRE(config.reserved_size == 2);
}
