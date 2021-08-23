#include <catch2/catch.hpp>

#include <string_view>

#include <paracooba/module.h>

#include "mocks.hpp"

static std::string test_qdimacs_str = R"(:p cnf 2 2
e 1 0
a 2 0
2 0
-2 0
)";

static std::string test_qdimacs_str_longer = R"(:p cnf 6 2
e 1 0
a 2 3 4 0
e 5 6 0
2 0
-2 0
)";

static std::string test_qdimacs_str_qbffam_parity_10 = R"(:p cnf 20 38
e 1 2 3 4 5 6 7 8 9 10 0
a 11 0
e 12 13 14 15 16 17 18 19 20 0
-1 -2 -12 0
-1 2 12 0
1 -2 12 0
1 2 -12 0
-12 -3 -13 0
-12 3 13 0
12 -3 13 0
12 3 -13 0
-13 -4 -14 0
-13 4 14 0
13 -4 14 0
13 4 -14 0
-14 -5 -15 0
-14 5 15 0
14 -5 15 0
14 5 -15 0
-15 -6 -16 0
-15 6 16 0
15 -6 16 0
15 6 -16 0
-16 -7 -17 0
-16 7 17 0
16 -7 17 0
16 7 -17 0
-17 -8 -18 0
-17 8 18 0
17 -8 18 0
17 8 -18 0
-18 -9 -19 0
-18 9 19 0
18 -9 19 0
18 9 -19 0
-19 -10 -20 0
-19 10 20 0
19 -10 20 0
19 10 -20 0
11 20 0
-11 -20 0)";

struct TestStruct {
  TestStruct(std::string_view s, int td, const char* name)
    : s(s)
    , td(td)
    , name(name) {}
  std::string_view s;
  int td;
  std::string name;
};

static std::vector<TestStruct> TestsVec = {
  TestStruct{ test_qdimacs_str, 2, "Small Test, td2" },
  TestStruct{ test_qdimacs_str_longer, 3, "Slightly Larger Test, td3" },
  TestStruct{ test_qdimacs_str_qbffam_parity_10,
              10,
              "Parity 10 from qbffam, td10" },
  TestStruct{ test_qdimacs_str_qbffam_parity_10,
              11,
              "Parity 10 from qbffam, td11" },
  TestStruct{ test_qdimacs_str_qbffam_parity_10,
              12,
              "Parity 10 from qbffam, td12" }
};

TEST_CASE("QBF formulas that go EXISTS 1 FORALL 2",
          "[integration][communicator][broker][solver][runner][solver_qbf]") {
  ParacWorkerCountSetter workerCount(GENERATE(1, 2));
  auto t = GENERATE(from_range(TestsVec));
  ParacSolverModuleSetter solverModSetter("./modules/solver_qbf/libparac_solver_qbf.so");
  ParacTreeDepthSetter tdSetter(t.td);

  CAPTURE(t.name);
  ParacoobaMock master(1, t.s.data());

  // Do NOT load the SAT solver module!
  std::string solverName = master.modules[PARAC_MOD_SOLVER]->name;
  REQUIRE(solverName != "cpp_cubetree_splitter");

  master.getThreadRegistry().wait();

  REQUIRE(master.exit_status == PARAC_UNSAT);
}
