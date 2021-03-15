#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/solver/types.hpp>

extern "C" {
#include <kissat/kissat.h>
}

namespace parac::solver {

#define PC_SHIFT(X) (((getter(i + X)) & 0b00000001) << (7 - X))
#define PC_SHIFT_CASE(X)                                                  \
  case X:                                                                 \
    b |= (((getter(i)) & 0b00000001) << (7 - (i % 8) - (8 - remaining))); \
    ++i;

class SolverAssignment {
  public:
  SolverAssignment() = default;
  ~SolverAssignment() = default;

  using UnpackedAssignmentVector = std::vector<Literal>;
  using PackedAssignmentVector = std::vector<uint8_t>;

  template<typename ValueGetter>
  inline void SerializeAssignment(const int varCount, ValueGetter getter) {
    assert(varCount > 0);
    m_varCount = varCount;
    m_packedAssignmentVector.resize((varCount - 1) / 8 + 1);
    size_t i = 0, pos = 0;
    // This should ignore the last remaining elements in a block of 8 entries.
    // The 1s, 2s, and 4s are therefore cut off.
    for(; i < (varCount & ~((size_t)0b00000111)); i += 8, ++pos) {
      uint8_t b = 0;
      b = PC_SHIFT(0) | PC_SHIFT(1) | PC_SHIFT(2) | PC_SHIFT(3) | PC_SHIFT(4) |
          PC_SHIFT(5) | PC_SHIFT(6) | PC_SHIFT(7);
      m_packedAssignmentVector[pos] = b;
    }

    if(varCount > 0) {
      uint8_t b = 0;
      size_t remaining = varCount - i;
      switch(remaining) {
        PC_SHIFT_CASE(7) [[fallthrough]];
        PC_SHIFT_CASE(6) [[fallthrough]];
        PC_SHIFT_CASE(5) [[fallthrough]];
        PC_SHIFT_CASE(4) [[fallthrough]];
        PC_SHIFT_CASE(3) [[fallthrough]];
        PC_SHIFT_CASE(2) [[fallthrough]];
        PC_SHIFT_CASE(1)
        m_packedAssignmentVector[pos] = b;
        [[fallthrough]];
        default:
          break;
      }
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
  template<class Solver>
  void SerializeAssignmentFromSolver(const int varCount, Solver& solver) {
    SerializeAssignment(varCount,
                        [&solver](int i) { return solver.val(i + 1); });
  }

  template<>
  void SerializeAssignmentFromSolver(const int varCount, kissat& solver) {
    SerializeAssignment(
      varCount, [&solver](int i) { return kissat_value(&solver, i + 1); });
  }

  void SerializeAssignmentFromArray(const int varCount,
                                    const UnpackedAssignmentVector& source) {
    SerializeAssignment(varCount, [&source](int i) { return source[i]; });
  }

  void DeSerializeSingleToAssignment(UnpackedAssignmentVector& target,
                                     uint8_t next,
                                     size_t n) {
    // Get next assignments, depending on remaining size. At the very end,
    // only valid variables shall be extracted.
    switch(n) {
      default:
        target.push_back(static_cast<bool>(next & 0b10000000u));
        [[fallthrough]];
      case 7:
        target.push_back(static_cast<bool>(next & 0b01000000u));
        [[fallthrough]];
      case 6:
        target.push_back(static_cast<bool>(next & 0b00100000u));
        [[fallthrough]];
      case 5:
        target.push_back(static_cast<bool>(next & 0b00010000u));
        [[fallthrough]];
      case 4:
        target.push_back(static_cast<bool>(next & 0b00001000u));
        [[fallthrough]];
      case 3:
        target.push_back(static_cast<bool>(next & 0b00000100u));
        [[fallthrough]];
      case 2:
        target.push_back(static_cast<bool>(next & 0b00000010u));
        [[fallthrough]];
      case 1:
        target.push_back(static_cast<bool>(next & 0b00000001u));
        [[fallthrough]];
      case 0:
        break;
    }
  }

  void DeSerializeToAssignment(UnpackedAssignmentVector& out) {
    out.reserve(m_varCount);

    size_t i;
    for(i = 0; m_varCount > 8; ++i, m_varCount -= 8) {
      DeSerializeSingleToAssignment(
        out, m_packedAssignmentVector[i], m_varCount);
    }

    DeSerializeSingleToAssignment(out, m_packedAssignmentVector[i], m_varCount);
  }

  size_t varCount() const { return m_varCount; }

  /** @brief Check if literal l is set. Literals start at 1. */
  bool isSet(Literal l) const {
    assert(l >= 1);
    l -= 1;
    assert(static_cast<size_t>(l) < m_varCount);
    return m_packedAssignmentVector[l / 8] & (l % 8);
  }
  bool operator[](Literal l) const { return isSet(l); }

  static bool static_isSet(void* assignmentVoid, Literal l) {
    assert(assignmentVoid);
    SolverAssignment* assignment =
      static_cast<SolverAssignment*>(assignmentVoid);
    return assignment->isSet(l);
  }
  static Literal static_highestLiteral(void* assignmentVoid) {
    assert(assignmentVoid);
    SolverAssignment* assignment =
      static_cast<SolverAssignment*>(assignmentVoid);
    return assignment->varCount();
  }

  private:
  PackedAssignmentVector m_packedAssignmentVector;
  size_t m_varCount;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("var_count", m_varCount), m_packedAssignmentVector);
  }
};

#undef PC_SHIFT
#undef PC_SHIFT_CASE

}
