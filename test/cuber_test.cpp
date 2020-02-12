#include <catch2/catch.hpp>
#include <paracuber/cuber/cuber.hpp>

using namespace paracuber;
using namespace paracuber::cuber;

TEST_CASE("Cuber initialise and transform LiteralOccurenceMap to LiteralMap")
{
  Cuber::LiteralOccurenceMap occMap;
  Cuber::initLiteralOccurenceMap(occMap, 5);
  REQUIRE(occMap[0].count == 0);
  REQUIRE(occMap[0].literal == 1);

  REQUIRE(occMap[4].count == 0);
  REQUIRE(occMap[4].literal == 5);

  occMap[1].count = 2;
  occMap[4].count = 3;

  Cuber::LiteralMap litMap;
  Cuber::literalOccurenceMapToLiteralMap(litMap, std::move(occMap));
  REQUIRE(litMap[0] == 5);
  REQUIRE(litMap[1] == 2);
  REQUIRE(litMap[2] == 1);
}
