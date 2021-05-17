#include <catch2/catch.hpp>

#include <paracooba/module.h>

#include "parser_qbf.hpp"
#include "qbf_parser_task.hpp"

static const char* test_qbf_formula = R"(:p cnf 4 2
a 1 2 0
e 3 4 0
-1  2 0
2 -3 -4 0)";

TEST_CASE("Parse a simple QBF Formula", "[solver_qbf]") {
  using namespace parac::solver_qbf;
  Parser parser;
  parac_status status = parser.prepare(test_qbf_formula);
  REQUIRE(status == PARAC_OK);
  status = parser.parse();
  REQUIRE(status == PARAC_OK);

  Parser::Quantifiers quantifiers = parser.quantifiers();
  Parser::Quantifiers quantifiersCheck = Parser::Quantifiers{
    Parser::Quantifier{ Parser::Quantifier::UNIVERSAL, 1 },
    Parser::Quantifier{ Parser::Quantifier::UNIVERSAL, 2 },
    Parser::Quantifier{ Parser::Quantifier::EXISTENTIAL, 3 },
    Parser::Quantifier{ Parser::Quantifier::EXISTENTIAL, 4 },
  };

  auto& lits = parser.literals();

  REQUIRE(quantifiers == quantifiersCheck);
  REQUIRE(lits == Parser::Literals{ -1, 2, 0, 2, -3, -4, 0 });
}

TEST_CASE("Parse a simple QBF Formula using QBFParserTask", "[solver_qbf]") {
  parac_handle handle;
  std::unique_ptr<parac_task, void (*)(parac_task*)> task(
    new parac_task, [](parac_task* t) {
      if(t->free_userdata) {
        t->free_userdata(t);
        delete t;
      }
    });
  bool passed = false;
  auto finished = [&passed](parac_status s,
                            std::unique_ptr<parac::solver_qbf::Parser> parser) {
    REQUIRE(s == PARAC_OK);
    REQUIRE(parser);
    passed = true;
  };

  new parac::solver_qbf::QBFParserTask(
    handle, *task, test_qbf_formula, finished);

  REQUIRE(task->work);

  parac_status status = task->work(task.get(), 1);
  REQUIRE(status == PARAC_OK);
  REQUIRE(passed);
}
