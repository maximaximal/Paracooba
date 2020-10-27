#pragma once

#include "paracooba/common/status.h"
#include <memory>
#include <string>

namespace CaDiCaL {
class Solver;
}

namespace parac::solver {
class CaDiCaLHandle {
  public:
  CaDiCaLHandle(bool &stop);
  ~CaDiCaLHandle();

  CaDiCaL::Solver& solver();

  parac_status parseFile(const std::string& path);

  bool hasFormula() const { return m_hasFormula; }

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  bool m_hasFormula;
};
}
