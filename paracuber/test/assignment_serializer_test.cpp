#include <catch2/catch.hpp>
#include <initializer_list>
#include <paracuber/assignment_serializer.hpp>

using namespace paracuber;

struct Solver
{
  explicit Solver(const std::vector<uint8_t>& assignments)
    : assignments(assignments)
  {}

  bool val(int lit) const
  {
    // Literals start at 1, as they also represent negations in SAT solvers.
    lit -= 1;

    REQUIRE(lit < getVarCount());
    return assignments[lit];
  }
  const std::vector<uint8_t> assignments;
  int getVarCount() const { return assignments.size(); }
};

struct Test
{
  explicit Test(std::initializer_list<uint8_t> assignments,
                std::initializer_list<uint8_t> expected)
    : assignments(assignments)
    , expected(expected)
    , solver(this->assignments)
  {}
  std::vector<uint8_t> assignments;
  std::vector<uint8_t> expected;
  Solver solver;
};

#define T true
#define F false

TEST_CASE("(Un-)Serialize SAT Assignment")
{
  auto test = GENERATE(
    Test({ T, F, F, F, T, T, T, T /* 1st byte */, F, F, F, T, T },
         { 0b10001111, 0b00000011 }),
    Test({ T, F, F, F, T, T, T, T /* 1st byte */, T },
         { 0b10001111, 0b00000001 }),
    Test({ T, F, F, F, T, T, T, T /* 1st byte */, F, F, F, F, F, F, F, T },
         { 0b10001111, 0b00000001 }),
    Test({ T, F, F, F, T, T, T, T /* 1st byte */,
           F, F, F, F, F, F, F, T /* 2nd byte */,
           F, T, F, T, F, T, F, T },
         { 0b10001111, 0b00000001, 0b01010101 }),
    Test({ T, F, F, F, T, T, T, T /* 1st byte */ }, { 0b10001111 }));

  uint32_t varCount = test.solver.getVarCount();
  CAPTURE(varCount);

  std::vector<uint8_t> actual((varCount - 1) / 8 + 1);

  SerializeAssignment(varCount, test.solver, actual);

  REQUIRE(test.expected == actual);

  // Other direction.
  auto calculatedAssignments = DeSerializeToAssignment(actual, varCount);
  REQUIRE(test.assignments == calculatedAssignments);
}
