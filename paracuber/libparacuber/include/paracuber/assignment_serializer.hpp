#ifndef PARACUBER_ASSIGNMENT_SERIALIZER
#define PARACUBER_ASSIGNMENT_SERIALIZER

#include <cstddef>
#include <cstdint>

namespace paracuber {

#define PC_SHIFT(X) (((solver.val(i + X)) & 0b00000001) << (7 - X))
#define PC_SHIFT_CASE(X)                                    \
  case X:                                                   \
    b |= (((solver.val(i)) & 0b00000001) << (7 - (i % 8))); \
    ++i;

template<class Solver, typename AssignmentVector>
inline void
SerializeAssignment(int varCount, Solver& solver, AssignmentVector& assignment)
{
  size_t i = 0, pos = 0;
  // This should ignore the last remaining elements in a block of 8 entries. The
  // 1s, 2s, and 4s are therefore cut off.
  for(; i < (varCount & ~((int)0b00000111)); i += 8, ++pos) {
    uint8_t b = 0;
    b = PC_SHIFT(0) | PC_SHIFT(1) | PC_SHIFT(2) | PC_SHIFT(3) | PC_SHIFT(4) |
        PC_SHIFT(5) | PC_SHIFT(6) | PC_SHIFT(7);
    assignment[pos] = b;
  }

  {
    uint8_t b = 0;
    switch(varCount - i) {
      PC_SHIFT_CASE(7)
      PC_SHIFT_CASE(6)
      PC_SHIFT_CASE(5)
      PC_SHIFT_CASE(4)
      PC_SHIFT_CASE(3)
      PC_SHIFT_CASE(2)
      PC_SHIFT_CASE(1)
      default:
        break;
    }
    assignment[pos] = b;
  }
}

#undef PC_SHIFT
#undef PC_SHIFT_CASE

}

#endif
