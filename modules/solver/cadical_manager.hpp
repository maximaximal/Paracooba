#pragma once

#include <memory>

#include <paracooba/common/types.h>
#include <paracooba/solver/types.hpp>

struct parac_module;
struct parac_task;
struct parac_path;
struct parac_compute_node;

namespace parac::solver {
class CaDiCaLHandle;
class SolverTask;
class CubeIteratorRange;
class SolverAssignment;
class SolverConfig;
class SatHandler;

namespace cubesource {
class Source;
}

class CaDiCaLManager {
  public:
  using CaDiCaLHandlePtr = std::unique_ptr<CaDiCaLHandle>;

  CaDiCaLManager(parac_module& mod,
                 CaDiCaLHandlePtr parsedFormula,
                 SolverConfig& solverConfig,
                 SatHandler& satHandler);
  ~CaDiCaLManager();

  SolverTask* createSolverTask(parac_task& task,
                               std::shared_ptr<cubesource::Source> cubesource);
  void deleteSolverTask(SolverTask* task);

  struct CaDiCaLHandlePtrWrapper {
    CaDiCaLHandlePtr ptr;
    CaDiCaLManager& mgr;
    parac_worker worker;

    explicit CaDiCaLHandlePtrWrapper(CaDiCaLHandlePtr ptr,
                                     CaDiCaLManager& mgr,
                                     parac_worker worker);

    CaDiCaLHandlePtrWrapper(CaDiCaLHandlePtrWrapper&& o) = default;
    CaDiCaLHandlePtrWrapper(const CaDiCaLHandlePtrWrapper& o) = delete;
    CaDiCaLHandlePtrWrapper(CaDiCaLHandlePtrWrapper& o) = delete;

    ~CaDiCaLHandlePtrWrapper();
  };

  CaDiCaLHandlePtrWrapper getHandleForWorker(parac_worker worker);

  void handleSatisfyingAssignmentFound(
    std::unique_ptr<SolverAssignment> assignment);

  CubeIteratorRange getCubeFromPath(parac_path path) const;

  /** @brief Get reference to solver module. */
  parac_module& mod() { return m_mod; }

  parac_id originatorId() const;

  const SolverConfig& config() const { return m_solverConfig; }

  const std::vector<std::shared_ptr<cubesource::Source>>& getRootCubeSources();

  const CaDiCaLHandle& parsedFormulaHandle() const;

  void updateAverageSolvingTime(double ms) const;
  double averageSolvingTimeMS();

  void addWaitingSolverTask();
  void removeWaitingSolverTask();
  size_t waitingSolverTasks() const;
  parac_id getOriginatorId() const;

  void addPossiblyNewNodePeer(parac_compute_node& peer);
  void applyAndDistributeNewLearnedClause(Clause c, parac_id source = 0);

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  parac_module& m_mod;
  CaDiCaLHandlePtr m_parsedFormula;
  SolverConfig& m_solverConfig;
  SatHandler& m_satHandler;

  CaDiCaLHandlePtr takeHandleForWorker(parac_worker worker);
  void returnHandleFromWorker(CaDiCaLHandlePtr handle, parac_worker worker);
};
}
