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
)";

TEST_CASE(
  "Connect one daemon and one master with two workers with a SAT formula",
  "[integration][communicator][broker][solver][runner][sat]") {
  ParacWorkerCountSetter workCount(2);
  ParacoobaMock master(1, test_dimacs_str);
  ParacoobaMock daemon1(2, nullptr, &master);

  master.getThreadRegistry().wait();

  REQUIRE(master.exit_status == PARAC_SAT);
  REQUIRE(master.assignment_data);
}

TEST_CASE(
  "Connect one daemon and one master without workers with a SAT formula",
  "[integration][communicator][broker][solver][runner][sat]") {
  ParacWorkerCountSetter workCount0(0);
  ParacoobaMock master(1, test_dimacs_str);
  ParacWorkerCountSetter workCount1(1);
  setenv("PARAC_WORKER_COUNT", "1", 1);
  ParacoobaMock daemon1(2, nullptr, &master);

  master.getThreadRegistry().wait();

  REQUIRE(master.exit_status == PARAC_SAT);
  REQUIRE(master.assignment_data);
}

TEST_CASE("Single master without daemons with SAT formula",
          "[integration][communicator][broker][solver][runner][sat]") {
  ParacWorkerCountSetter workCount1(1);
  ParacoobaMock master(1, test_dimacs_str);

  master.getThreadRegistry().wait();

  REQUIRE(master.exit_status == PARAC_SAT);
  REQUIRE(master.assignment_data);
}

TEST_CASE("Single master with two threads without daemons with SAT formula",
          "[integration][communicator][broker][solver][runner][sat]") {
  ParacWorkerCountSetter workCount2(2);
  ParacoobaMock master(1, test_dimacs_str);

  master.getThreadRegistry().wait();

  REQUIRE(master.exit_status == PARAC_SAT);
  REQUIRE(master.assignment_data);
}
