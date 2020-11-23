#pragma once

#include "paracooba/common/status.h"
#include "paracooba/common/types.h"
#include "paracooba/solver/types.hpp"
#include <memory>
#include <string>

struct parac_path;

namespace CaDiCaL {
class Solver;
}

namespace parac::solver {
class CubeIteratorRange;
class SolverAssignment;

class CaDiCaLHandle {
  public:
  CaDiCaLHandle(bool& stop, parac_id originatorId);
  CaDiCaLHandle(CaDiCaLHandle& o);
  ~CaDiCaLHandle();

  CaDiCaL::Solver& solver();

  parac_status parseFile(const std::string& path);

  bool hasFormula() const { return m_hasFormula; }

  const std::string& path() const;
  parac_id originatorId() const;

  CubeIteratorRange getCubeFromId(CubeId id) const;
  CubeIteratorRange getCubeFromPath(parac_path path) const;

  void applyCubeAsAssumption(CubeIteratorRange cube);

  /** @brief Calls solve on the internal CaDiCaL Solver instance.
   *
   * @returns PARAC_ABORTED, PARAC_SAT, PARAC_UNSAT, or PARAC_UNKNOWN
   */
  parac_status solve();

  std::unique_ptr<SolverAssignment> takeSolverAssignment();

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;
  std::unique_ptr<SolverAssignment> m_solverAssignment;

  void generateJumplist();

  bool m_hasFormula = false;
};
}
