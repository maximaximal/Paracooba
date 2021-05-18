#pragma once

#include <memory>
#include <string>

#include <paracooba/common/status.h>

#include "qbf_types.hpp"

namespace parac::solver_qbf {
class Parser;

class GenericSolverHandle {
  public:
  GenericSolverHandle();
  virtual ~GenericSolverHandle();

  virtual void assumeCube(const Cube& cube) = 0;

  virtual parac_status solve() = 0;

  private:
  std::string m_path;
};
}
