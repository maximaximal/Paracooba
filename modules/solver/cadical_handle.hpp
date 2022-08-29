#pragma once

#include "paracooba/common/status.h"
#include "paracooba/common/types.h"
#include "paracooba/solver/types.hpp"
#include "portfolio_sat_handle.hpp"
#include <memory>
#include <string>

struct parac_path;
struct parac_handle;
struct parac_timeout;
struct parac_task;

namespace CaDiCaL {
class Solver;
}

namespace parac::solver {
struct CubeIteratorRange;
class SolverAssignment;
class SolverConfig;

class CaDiCaLHandle {
  public:
  CaDiCaLHandle(parac_handle& handle,
                volatile bool& stop,
                parac_id originatorId);
  CaDiCaLHandle(CaDiCaLHandle& o, const SolverConfig& cfg);
  ~CaDiCaLHandle();

  CaDiCaL::Solver& solver();

  std::pair<parac_status, std::string> prepareString(std::string_view iCNF);
  parac_status parseFile(const std::string& path);

  bool hasFormula() const { return m_hasFormula; }

  const std::string& path() const;
  parac_id originatorId() const;

  CubeIteratorRange getCubeFromId(CubeId id) const;
  CubeIteratorRange getCubeFromPath(parac_path path) const;
  size_t getPregeneratedCubesCount() const;
  size_t getNormalizedPathLength() const;

  void applyCubeAsAssumption(const CubeIteratorRange& cube);
  void applyCubeAsAssumption(const Cube& cube);

  void applyLearnedClause(const Clause& clause);
  bool stoppedGlobally() const;

  /** @brief Calls solve on the internal CaDiCaL Solver instance.
   *
   * @returns PARAC_ABORTED, PARAC_SAT, PARAC_UNSAT, or PARAC_UNKNOWN
   */
  parac_status solve(parac_task& task);

  void terminate();

  /** @brief Try to resplit the current cube.
   *
   * The returned pair is (left, right)
   */
  std::pair<parac_status, std::optional<std::pair<Cube, Cube>>> resplitOnce(
    parac_path path,
    Cube literals);

  /** @brief Resplit a provided cube and return the literal to split on.
   *
   * The split must then be -lit and +lit.
   */
  std::pair<parac_status, Literal>
  resplitCube(parac_path p, Cube currentCube, const SolverConfig& solverConfig);

  parac_status lookahead(size_t depth, size_t min_depth);

  struct FastLookaheadResult {
    parac_status status;
    std::vector<Cube> cubes;
  };

  FastLookaheadResult fastLookahead(size_t depth);

  std::unique_ptr<SolverAssignment> takeSolverAssignment();

  private:
  struct Internal;
  std::shared_ptr<Internal> m_internal;
  std::unique_ptr<PortfolioSATHandle> m_portfolioSATHandle;
  std::reference_wrapper<GenericSolverHandle> m_solverHandle;

  void generateJumplist();

  bool m_hasFormula = false;
  bool m_interruptedLookahead = false;

  parac_timeout* m_lookaheadTimeout = nullptr;
};
}
