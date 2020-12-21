#include <catch2/catch.hpp>

#include <paracooba/module.h>

#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "cube_source.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/path.h"
#include "solver_config.hpp"

using namespace parac::solver;
using namespace parac::solver::cubesource;

static const char* test_dimacs_str = R"(
p inccnf
-1 2 0
2 1 0
-1 1 0
a 1 2 0
a 1 -2 0
a -1 0
)";

TEST_CASE("Test PathDefined Cube-Source", "[solver][cubesource]") {
  parac_log_init(nullptr);

  parac_handle handle;
  parac_module mod;
  bool stop = false;
  SolverConfig dummyConfig;
  std::unique_ptr<CaDiCaLHandle> dummyHandle =
    std::make_unique<CaDiCaLHandle>(handle, stop, 1);
  auto [s, outFile] = dummyHandle->prepareString(test_dimacs_str);
  dummyHandle->parseFile(outFile);

  REQUIRE(s == PARAC_OK);

  CaDiCaLManager dummyManager(mod, std::move(dummyHandle), dummyConfig);

  const auto& f = dummyManager.parsedFormulaHandle();
  REQUIRE(f.getPregeneratedCubesCount() == 3);
  REQUIRE(f.getCubeFromId(0).size() == 2);
  REQUIRE(f.getCubeFromId(1).size() == 2);
  REQUIRE(f.getCubeFromId(2).size() == 1);
  REQUIRE(f.getCubeFromId(3).size() == 0);

  [[maybe_unused]] auto pROOT = parac_path_build((uint8_t)0b00000000u, 0);
  [[maybe_unused]] auto p0 = parac_path_build((uint8_t)0b00000000u, 1);
  [[maybe_unused]] auto p1 = parac_path_build((uint8_t)0b10000000u, 1);
  [[maybe_unused]] auto p00 = parac_path_build((uint8_t)0b00000000u, 2);
  [[maybe_unused]] auto p01 = parac_path_build((uint8_t)0b01000000u, 2);
  [[maybe_unused]] auto p10 = parac_path_build((uint8_t)0b10000000u, 2);
  [[maybe_unused]] auto p11 = parac_path_build((uint8_t)0b11000000u, 2);

  REQUIRE(f.getCubeFromPath(parac_path_build(0b00, 2)).size() == 2);
  REQUIRE(f.getCubeFromPath(parac_path_build(0b01, 2)).size() == 2);
  REQUIRE(f.getCubeFromPath(parac_path_build(0b10, 2)).size() == 2);

  PathDefined pathDefined;

  bool left, right;
  CHECK(pathDefined.split(pROOT, dummyManager, f, left, right));
  CHECK(left);
  CHECK(right);

  CHECK(pathDefined.split(p0, dummyManager, f, left, right));
  CHECK(left);
  CHECK(right);

  CHECK(pathDefined.split(p1, dummyManager, f, left, right));
  CHECK(left);
  CHECK(!right);
}
