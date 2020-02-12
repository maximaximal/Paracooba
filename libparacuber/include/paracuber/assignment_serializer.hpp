#ifndef PARACUBER_ASSIGNMENT_SERIALIZER
#define PARACUBER_ASSIGNMENT_SERIALIZER

#include <cstddef>
#include <cstdint>
#include <vector>

namespace paracuber {

#define PC_SHIFT(X) (((getter(i + X)) & 0b00000001) << (7 - X))
#define PC_SHIFT_CASE(X)                                                  \
  case X:                                                                 \
    b |= (((getter(i)) & 0b00000001) << (7 - (i % 8) - (8 - remaining))); \
    ++i;

template<typename AssignmentVector, typename ValueGetter>
inline void
SerializeAssignment(AssignmentVector& target,
                    const int varCount,
                    ValueGetter getter)
{
  target.resize((varCount - 1) / 8 + 1);
  size_t i = 0, pos = 0;
  // This should ignore the last remaining elements in a block of 8 entries. The
  // 1s, 2s, and 4s are therefore cut off.
  for(; i < (varCount & ~((int)0b00000111)); i += 8, ++pos) {
    uint8_t b = 0;
    b = PC_SHIFT(0) | PC_SHIFT(1) | PC_SHIFT(2) | PC_SHIFT(3) | PC_SHIFT(4) |
        PC_SHIFT(5) | PC_SHIFT(6) | PC_SHIFT(7);
    target[pos] = b;
  }

  if(varCount > 0) {
    uint8_t b = 0;
    size_t remaining = varCount - i;
    switch(remaining) {
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
    target[pos] = b;
  } else {
    // It is very unlikely this ever happens, there should be some variables.
    // No error handling is done here though, this should be handled from
    // outside.
  }
}

/** @brief Serialise the given solver results into an assignment vector.
 *
 * Encodes literals sequentially into bytes. Each byte has 8 literals,
 * represented as bits. A set bit is a set literal, an unset bit is an unset
 * literal. The last byte is encoded with a padding on the left side to make
 * parsing easier.
 *
 * All literals are inserted left-to-right, like this:
 * BYTE:     0b00000000
 * Literals:   12345678
 *
 * Literals start at 1!
 */
template<class Solver, typename AssignmentVector>
inline void
SerializeAssignmentFromSolver(AssignmentVector& target,
                              const int varCount,
                              Solver& solver)
{
  SerializeAssignment(
    target, varCount, [&solver](int i) { return solver.val(i + 1); });
}

template<typename AssignmentVector, typename SourceArray>
inline void
SerializeAssignmentFromArray(AssignmentVector& target,
                              const int varCount,
                              const SourceArray& source)
{
  SerializeAssignment(target, varCount, [&source](int i) { return source[i]; });
}

template<typename AssignmentVector>
inline void
DeSerializeSingleToAssignment(AssignmentVector& target, uint8_t next, size_t n)
{
  // Get next assignments, depending on remaining size. At the very end,
  // only valid variables shall be extracted.
  switch(n) {
    default:
      target.push_back(static_cast<bool>(next & 0b10000000u));
    case 7:
      target.push_back(static_cast<bool>(next & 0b01000000u));
    case 6:
      target.push_back(static_cast<bool>(next & 0b00100000u));
    case 5:
      target.push_back(static_cast<bool>(next & 0b00010000u));
    case 4:
      target.push_back(static_cast<bool>(next & 0b00001000u));
    case 3:
      target.push_back(static_cast<bool>(next & 0b00000100u));
    case 2:
      target.push_back(static_cast<bool>(next & 0b00000010u));
    case 1:
      target.push_back(static_cast<bool>(next & 0b00000001u));
    case 0:
      break;
  }
}

template<typename AssignmentVector>
inline void
DeSerializeToAssignment(AssignmentVector& out,
                        AssignmentVector& source,
                        size_t varCount)
{
  out.reserve(varCount);

  size_t i;
  for(i = 0; varCount > 8; ++i, varCount -= 8) {
    DeSerializeSingleToAssignment(out, source[i], varCount);
  }

  DeSerializeSingleToAssignment(out, source[i], varCount);
}

#undef PC_SHIFT
#undef PC_SHIFT_CASE

}

#endif
