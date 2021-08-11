#include <catch2/catch.hpp>

#include <paracooba/module.h>

#include "mocks.hpp"

static const char* test_qdimacs_str = R"(:p cnf 2 2
e 1 0
a 2 0
2 0
-2 0
)";

TEST_CASE("Single threaded QBF formula that goes EXISTS 1 FORALL 2",
          "[integration][communicator][broker][solver][runner][qbf]") {
  ParacWorkerCountSetter workerCount(1);

  setenv("PARAC_MODULEPATH_SOLVER", "./modules/solver_qbf/", 1);
  ParacoobaMock master(1, test_qdimacs_str);
  unsetenv("PARAC_MODULEPATH_SOLVER");

  // Do NOT load the SAT solver module!
  std::string solverName = master.modules[PARAC_MOD_SOLVER]->name;
  REQUIRE(solverName != "cpp_cubetree_splitter");

  master.getThreadRegistry().wait();

  REQUIRE(master.exit_status == PARAC_UNSAT);
}
