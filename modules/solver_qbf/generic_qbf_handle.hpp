#pragma once

#include <memory>
#include <string>

#include <paracooba/common/status.h>
#include <paracooba/solver/cube_iterator.hpp>

#include "qbf_types.hpp"

namespace parac::solver_qbf {
class Parser;
using CubeIteratorRange = solver::CubeIteratorRange;

class GenericSolverHandle {
  public:
  GenericSolverHandle();
  virtual ~GenericSolverHandle();

  virtual void assumeCube(const CubeIteratorRange& cube) = 0;

  virtual parac_status solve() = 0;

  virtual void terminate() = 0;

  virtual const char* name() const = 0;

  private:
  std::string m_path;
};
}
