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
}

TEST_CASE("CNFTree Usage")
{
  CNFTree tree;

  SECTION("Traversing an empty tree should not call the visitor.")
  {
    uint8_t depth = GENERATE(0, 1, 2, 3);

    tree.visit(CNFTree::buildPath((uint8_t)0b10000000, depth),
               [](CNFTree::CubeVar l, uint8_t d) {
                 REQUIRE(false);
                 return false;
               });
  }

  // Setup the tree.
  REQUIRE(tree.setDecision(CNFTree::buildPath((uint8_t)0b00000000, 1), 1));
  REQUIRE(tree.setDecision(CNFTree::buildPath((uint8_t)0b00000000, 2), 2));
  REQUIRE(tree.setDecision(CNFTree::buildPath((uint8_t)0b10000000, 2), 3));

  SECTION(
    "Visitor should be called for a traversed node with the correct arguments.")
  {
    {
      std::vector<CNFTree::CubeVar> vars;
      REQUIRE(tree.visit(CNFTree::buildPath((uint8_t)0b10000000, 1),
                         [&vars](CNFTree::CubeVar l, uint8_t d) {
                           vars.push_back(l);
                           return false;
                         }));
      REQUIRE(vars.size() == 1);
      REQUIRE(vars[0] == 1);
    }

    {
      CNFTree::Path p = CNFTree::buildPath((uint8_t)0b10000000, 2);

      std::vector<CNFTree::CubeVar> vars;
      REQUIRE(tree.visit(p, [&vars](CNFTree::CubeVar l, uint8_t d) {
        vars.push_back(l);
        return false;
      }));
      REQUIRE(vars.size() == 2);
      REQUIRE(vars[0] == 1);
      REQUIRE(vars[1] == -3);

      REQUIRE(tree.writePathToContainer(vars, p));
      REQUIRE(vars[0] == 1);
      REQUIRE(vars[1] == -3);
    }

    {
      std::vector<CNFTree::CubeVar> vars;
      REQUIRE(!tree.visit(CNFTree::buildPath((uint8_t)0b10000000, 4),
                          [&vars](CNFTree::CubeVar l, uint8_t d) {
                            vars.push_back(l);
                            return false;
                          }));
      REQUIRE(vars.size() == 2);
    }
  }
}
