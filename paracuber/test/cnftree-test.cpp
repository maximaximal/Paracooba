#include <catch2/catch.hpp>
#include <paracuber/cnftree.hpp>

using namespace paracuber;

TEST_CASE("CNFTree::Path manipulation")
{
  CNFTree::Path p = 0;
  REQUIRE(CNFTree::getDepth(p) == 0);
  REQUIRE(CNFTree::getPath(p) == 0);

  p = CNFTree::setDepth(p, 1);
  REQUIRE(CNFTree::getDepth(p) == 1);
  REQUIRE(CNFTree::getPath(p) == 0);

  p = CNFTree::setDepth(p, 2);
  REQUIRE(!CNFTree::getAssignment(p, 1));
  REQUIRE(!CNFTree::getAssignment(p, 2));
  REQUIRE(!CNFTree::getAssignment(p, 3));

  p = CNFTree::buildPath(0, 2);
  REQUIRE(CNFTree::getPath(p) == 0);
  p = CNFTree::buildPath(0xFFFF0000FFFF0000u, 2);
  REQUIRE(CNFTree::getPath(p) == 0xFFFF0000FFFF0000u);
  p = CNFTree::buildPath((uint8_t)0b00000001u, 2);
  REQUIRE(CNFTree::getPath(p) == ((uint64_t)0b00000001u) << (64 - 8));
  REQUIRE(!CNFTree::getAssignment(p, 1));
  p = CNFTree::buildPath((uint8_t)0b10000000u, 2);
  REQUIRE(CNFTree::getPath(p) == ((uint64_t)0b10000000u) << (64 - 8));

  REQUIRE(CNFTree::getDepth(p) == 2);
  REQUIRE(CNFTree::getAssignment(p, 1));
  REQUIRE(!CNFTree::getAssignment(p, 2));
  REQUIRE(CNFTree::getDepthShiftedPath(p) == (uint64_t)0b00000010uL);

  p = CNFTree::buildPath((uint8_t)0b10000000u, 4);
  REQUIRE(CNFTree::getDepthShiftedPath(p) == (uint64_t)0b00001000uL);
  REQUIRE(CNFTree::getDepthShiftedPath(p) == 8u);

  REQUIRE(CNFTree::getDepth(CNFTree::getNextLeftPath(p)) == 5);
  REQUIRE(!CNFTree::getAssignment(CNFTree::getNextLeftPath(p), 5));
  REQUIRE(CNFTree::getAssignment(CNFTree::getNextRightPath(p), 5));
  REQUIRE(CNFTree::getDepthShiftedPath(CNFTree::getNextLeftPath(p)) ==
          (uint64_t)0b00010000uL);
  REQUIRE(CNFTree::getDepthShiftedPath(CNFTree::getNextRightPath(p)) ==
          (uint64_t)0b00010001uL);

  // This is also a test for cuber::Cuber::getAdditionComponent. The behaviour
  // stays the same.
  p = CNFTree::buildPath((uint8_t)0b11110000u, 4);
  REQUIRE(CNFTree::getDepthShiftedPath(p) == 15u);
  REQUIRE(CNFTree::maxPathDepth == 58u);

  p = CNFTree::setDepth(CNFTree::setAssignment(p, 5, true), 5);
  REQUIRE(CNFTree::getPath(p) == ((uint64_t)0b11111000u) << (64 - 8));
  REQUIRE(CNFTree::getDepthShiftedPath(p) == 31u);
}

TEST_CASE("CNFTree Usage")
{
  CNFTree tree(nullptr, 0);

  SECTION("Traversing an empty tree should not call the visitor.")
  {
    uint8_t depth = GENERATE(0, 1, 2, 3);

    tree.visit(
      CNFTree::buildPath((uint8_t)0b10000000, depth),
      [](CNFTree::CubeVar l, uint8_t d, CNFTree::State state, int64_t remote) {
        REQUIRE(false);
        return false;
      });
  }

  // Setup the tree.
  REQUIRE(tree.setDecision(CNFTree::buildPath((uint8_t)0b00000000, 0), 1));
  REQUIRE(tree.setDecision(CNFTree::buildPath((uint8_t)0b00000000, 1), 2));
  REQUIRE(tree.setDecision(CNFTree::buildPath((uint8_t)0b10000000, 1), 3));

  SECTION(
    "Visitor should be called for a traversed node with the correct arguments.")
  {
    {
      std::vector<CNFTree::CubeVar> vars;
      REQUIRE(tree.visit(
        CNFTree::buildPath((uint8_t)0b10000000, 1),
        [&vars](
          CNFTree::CubeVar l, uint8_t d, CNFTree::State state, int64_t remote) {
          REQUIRE(state == CNFTree::Unvisited);
          vars.push_back(l);
          return false;
        }));
      REQUIRE(vars.size() == 2);
      REQUIRE(vars[0] == 1);
    }

    {
      CNFTree::Path p = CNFTree::buildPath((uint8_t)0b10000000, 2);

      std::vector<CNFTree::CubeVar> vars;
      REQUIRE(tree.visit(
        p,
        [&vars](
          CNFTree::CubeVar l, uint8_t d, CNFTree::State state, int64_t remote) {
          vars.push_back(l);
          return false;
        }));
      REQUIRE(vars.size() == 2);
      REQUIRE(vars[0] == 1);
      REQUIRE(vars[1] == -3);

      REQUIRE(tree.writePathToLiteralContainer(vars, p));
      REQUIRE(vars[0] == 1);
      REQUIRE(vars[1] == -3);
    }

    {
      std::vector<CNFTree::CubeVar> vars;
      REQUIRE(!tree.visit(
        CNFTree::buildPath((uint8_t)0b10000000, 4),
        [&vars](
          CNFTree::CubeVar l, uint8_t d, CNFTree::State state, int64_t remote) {
          vars.push_back(l);
          return false;
        }));
      REQUIRE(vars.size() == 2);
    }
  }
}
