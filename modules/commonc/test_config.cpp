#include <catch2/catch.hpp>

#include <paracooba/common/config.h>

TEST_CASE("Config Initialisation, Reserving, Registration and Free",
          "[commonc][config]") {
  parac_config config;
  parac_config_init(&config);

  std::size_t entryCount = 2;

  parac_config_entry entries[entryCount];

  REQUIRE(parac_config_reserve(&config, entryCount) == PARAC_OK);

  REQUIRE(config.size == 0);
  REQUIRE(config.reserved_size == 2);

  for(auto& e : entries) {
    CAPTURE(config.size);
    CAPTURE(config.reserved_size);
    REQUIRE(parac_config_register(&config, &e) == PARAC_OK);
  }

  REQUIRE(config.size == 2);
  REQUIRE(config.reserved_size == 2);

  REQUIRE(parac_config_register(&config, &entries[1]) == PARAC_FULL);

  REQUIRE(config.entries[1] == &entries[1]);

  REQUIRE(parac_config_reserve(&config, 1) == PARAC_OK);

  REQUIRE(config.size == 2);
  REQUIRE(config.reserved_size == 3);

  parac_config_entry e3;
  REQUIRE(parac_config_register(&config, &e3) == PARAC_OK);

  REQUIRE(config.size == 3);
  REQUIRE(config.reserved_size == 3);

  parac_config_free(&config);

  REQUIRE(config.size == 0);
  REQUIRE(config.reserved_size == 0);
}
