#include <catch2/catch.hpp>
#include <paracuber/assignment_serializer.hpp>

using namespace paracuber;

struct Solver
{
  bool val(int lit) const
  {
    // Literals start at 1, as they also represent negations in SAT solvers.
    lit -= 1;

    REQUIRE(lit < getVarCount());
    return assignment[lit];
  }
  std::vector<bool> assignment;
  int getVarCount() const { return assignment.size(); }
};

struct Test
{
  Solver solver;
  std::vector<uint8_t> expected;
};

#define T true
#define F false

TEST_CASE("Serialize SAT Assignment")
{
  auto test = GENERATE(
    Test{ { { T, F, F, F, T, T, T, T /* 1st byte */, F, F, F, T, T } },
          { 0b10001111, 0b00011000 } },
    Test{ { { T, F, F, F, T, T, T, T /* 1st byte */, T } },
          { 0b10001111, 0b10000000 } },
    Test{ { { T, F, F, F, T, T, T, T /* 1st byte */, F, F, F, F, F, F, F, T } },
          { 0b10001111, 0b00000001 } },
    Test{ { { T, F, F, F, T, T, T, T /* 1st byte */,
              F, F, F, F, F, F, F, T /* 2nd byte */,
              F, T, F, T, F, T, F, T } },
          { 0b10001111, 0b00000001, 0b01010101 } },
    Test{ { { T, F, F, F, T, T, T, T /* 1st byte */ } }, { 0b10001111 } });

  std::vector<uint8_t> actual((test.solver.getVarCount() - 1) / 8 + 1);

  SerializeAssignment(test.solver.getVarCount(), test.solver, actual);

  REQUIRE(test.expected == actual);
}
