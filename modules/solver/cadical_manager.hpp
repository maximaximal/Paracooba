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

class CaDiCaLManager {
  public:
  using CaDiCaLHandlePtr = std::unique_ptr<CaDiCaLHandle>;

  CaDiCaLManager(parac_module& mod, CaDiCaLHandlePtr parsedFormula);
  ~CaDiCaLManager();

  SolverTask* createSolverTask(parac_task& task);
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

  CubeIteratorRange getCubeFromPath(parac_path path) const;

  /** @brief Get reference to solver module. */
  parac_module& mod() { return m_mod; }

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  parac_module& m_mod;
  CaDiCaLHandlePtr m_parsedFormula;

  CaDiCaLHandlePtr takeHandleForWorker(parac_worker worker);
  void returnHandleFromWorker(CaDiCaLHandlePtr handle, parac_worker worker);
};
}
