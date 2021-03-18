#include <catch2/catch.hpp>

#include <string>

#include <paracooba/module.h>

#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "cube_source.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/path.h"
#include "sat_handler.hpp"
#include "solver_config.hpp"

using namespace parac::solver;
using namespace parac::solver::cubesource;

using std::to_string;

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
  memset(&handle, 0, sizeof(parac_handle));
  handle.input_file = nullptr;
  handle.modules[PARAC_MOD_RUNNER] = nullptr;
  parac_module mod;
  mod.handle = &handle;
  bool stop = false;
  SolverConfig dummyConfig;
  std::unique_ptr<CaDiCaLHandle> dummyHandle =
    std::make_unique<CaDiCaLHandle>(handle, stop, 1);
  auto [s, outFile] = dummyHandle->prepareString(test_dimacs_str);
  dummyHandle->parseFile(outFile);

  SatHandler dummySatHandler(mod, handle.id);

  REQUIRE(s == PARAC_OK);

  CaDiCaLManager dummyManager(
    mod, std::move(dummyHandle), dummyConfig, dummySatHandler);

  const auto& f = dummyManager.parsedFormulaHandle();
  REQUIRE(f.getPregeneratedCubesCount() == 3);
  REQUIRE(f.getCubeFromId(0).size() == 2);
  REQUIRE(f.getCubeFromId(1).size() == 2);
  REQUIRE(f.getCubeFromId(2).size() == 1);
  REQUIRE(f.getCubeFromId(3).size() == 0);

  [[maybe_unused]] parac::Path pROOT =
    parac_path_build((uint8_t)0b00000000u, 0);
  [[maybe_unused]] parac::Path p0 = parac_path_build((uint8_t)0b00000000u, 1);
  [[maybe_unused]] parac::Path p1 = parac_path_build((uint8_t)0b10000000u, 1);
  [[maybe_unused]] parac::Path p00 = parac_path_build((uint8_t)0b00000000u, 2);
  [[maybe_unused]] parac::Path p01 = parac_path_build((uint8_t)0b01000000u, 2);
  [[maybe_unused]] parac::Path p10 = parac_path_build((uint8_t)0b10000000u, 2);
  [[maybe_unused]] parac::Path p11 = parac_path_build((uint8_t)0b11000000u, 2);

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
