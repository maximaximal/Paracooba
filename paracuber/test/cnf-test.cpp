#include <catch2/catch.hpp>
#include <paracuber/cnf.hpp>

using namespace paracuber;

TEST_CASE("Send and receive a CNF formula with previous = 1")
{
  CNF from(1, "");
  CNF to;

  REQUIRE(to.getPrevious() == -1);
  REQUIRE(from.getPrevious() == 1);
}

TEST_CASE("Send and receive a CNF formula with previous = 0") {}
