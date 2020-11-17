#pragma once

#include <memory>
#include <vector>

class parac_module;

namespace parac::solver {
class CaDiCaLHandle;

class CaDiCaLManager {
  public:
  using CaDiCaLHandlePtr = std::unique_ptr<CaDiCaLHandle>;

  CaDiCaLManager(parac_module& mod, CaDiCaLHandlePtr parsedFormula);
  ~CaDiCaLManager();

  private:
  CaDiCaLHandlePtr m_parsedFormula;
  std::vector<CaDiCaLHandlePtr> m_solvers;
};
}
