#include <catch2/catch.hpp>

#include "parser_qbf.hpp"

static const char* test_qbf_formula = R"(:
p cnf 4 2
e 1 2 3 4 0
-1  2 0
2 -3 -4 0)";

TEST_CASE("Parse a simple QBF Formula", "[solver_qbf]") {
  using namespace parac::solver_qbf;
  Parser parser;
  parac_status status = parser.parse(test_qbf_formula);
  REQUIRE(status == PARAC_OK);
}
