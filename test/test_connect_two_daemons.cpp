#include "mocks.hpp"

#include <catch2/catch.hpp>
#include <chrono>
#include <thread>

#include <paracooba/common/compute_node_store.h>

TEST_CASE("Connect two daemons.", "[integration,communicator,broker]") {
  ParacoobaMock master(1);
  ParacoobaMock daemon(2, &master);

  auto master_cns = master.getBroker().compute_node_store;
  auto daemon_cns = daemon.getBroker().compute_node_store;

  REQUIRE(master_cns->has(master_cns, 1));
  REQUIRE(!master_cns->has(master_cns, 2));
  REQUIRE(!daemon_cns->has(daemon_cns, 1));
  REQUIRE(daemon_cns->has(daemon_cns, 2));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  REQUIRE(master_cns->has(master_cns, 1));
  REQUIRE(master_cns->has(master_cns, 2));
  REQUIRE(daemon_cns->has(daemon_cns, 1));
  REQUIRE(daemon_cns->has(daemon_cns, 2));
}
