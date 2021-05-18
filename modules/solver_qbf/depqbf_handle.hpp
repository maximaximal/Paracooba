#pragma once

#include "generic_qbf_handle.hpp"

namespace parac::solver_qbf {
class Parser;

class DepQBFHandle : public GenericSolverHandle {
  public:
  DepQBFHandle(const Parser& parser);
  virtual ~DepQBFHandle();

  virtual void assumeCube(const Cube &cube);
  virtual parac_status solve();

  private:
};
}
