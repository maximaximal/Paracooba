#pragma once

#include <memory>

#include <paracooba/common/types.h>

class parac_module;
class parac_task;

namespace parac::solver {
class CaDiCaLHandle;
class SolverTask;

class CaDiCaLManager {
  public:
  using CaDiCaLHandlePtr = std::unique_ptr<CaDiCaLHandle>;

  CaDiCaLManager(parac_module& mod, CaDiCaLHandlePtr parsedFormula);
  ~CaDiCaLManager();

  SolverTask* createSolverTask(parac_task& task);
  void deleteSolverTask(SolverTask* task);

  CaDiCaLHandlePtr takeHandleForWorker(parac_worker worker);
  void returnHandleFromWorker(CaDiCaLHandlePtr handle, parac_worker worker);

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  parac_module& m_mod;
  CaDiCaLHandlePtr m_parsedFormula;
};
}
