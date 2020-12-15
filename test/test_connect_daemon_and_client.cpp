#include "paracooba/module.h"
#define PARAC_LOG_INCLUDE_FMT

#include "mocks.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/status.h"

#include "paracooba/common/log.h"

#include <catch2/catch.hpp>
#include <chrono>
#include <thread>

#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/message.h>
#include <paracooba/common/thread_registry.h>

static const char* test_dimacs_str = R"(:
p inccnf
-1 2 0
2 1 0
-2 0
2 0
1 0
-1 0
-1 1 0
1 -1 0
a 1 2 0
a 1 -2 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
a -1 0
)";

TEST_CASE("Connect multiple daemon and one master without workers.",
          "[integration,communicator,broker,solver,runner]") {
  setenv("PARAC_WORKER_COUNT", "0", 1);
  ParacoobaMock master(1, test_dimacs_str);
  setenv("PARAC_WORKER_COUNT", "1", 1);
  ParacoobaMock daemon1(2, nullptr, &master);
  ParacoobaMock daemon2(3, nullptr, &master);
  ParacoobaMock daemon3(4, nullptr, &master);
  ParacoobaMock daemon4(5, nullptr, &master);
  setenv("PARAC_WORKER_COUNT",
         std::to_string(std::thread::hardware_concurrency()).c_str(),
         1);

  master.getThreadRegistry().wait();

  REQUIRE(master.exit_status == PARAC_UNSAT);
}
