#pragma once

#include "paracooba/common/status.h"
#include "paracooba/common/types.h"
#include <memory>
#include <string>

namespace CaDiCaL {
class Solver;
}

namespace parac::solver {
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

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  bool m_hasFormula = false;
};
}
