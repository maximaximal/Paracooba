#pragma once

#include <memory>

#include <paracooba/common/types.h>
#include <paracooba/solver/types.hpp>

struct parac_module;
struct parac_task;
struct parac_path;

namespace parac::solver {
class CaDiCaLHandle;
class SolverTask;
class CubeIteratorRange;
class SolverAssignment;
class SolverConfig;

namespace cubesource {
class Source;
}

class CaDiCaLManager {
  public:
  using CaDiCaLHandlePtr = std::unique_ptr<CaDiCaLHandle>;

  CaDiCaLManager(parac_module& mod,
                 CaDiCaLHandlePtr parsedFormula,
                 SolverConfig& solverConfig);
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

  std::unique_ptr<cubesource::Source> createRootCubeSource();

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;
  std::unique_ptr<SolverAssignment> m_solverAssignment;

  parac_module& m_mod;
  CaDiCaLHandlePtr m_parsedFormula;
  SolverConfig& m_solverConfig;

  CaDiCaLHandlePtr takeHandleForWorker(parac_worker worker);
  void returnHandleFromWorker(CaDiCaLHandlePtr handle, parac_worker worker);
};
}
