#pragma once

#include <memory>

#include "generic_qbf_handle.hpp"

struct QDPLL;

namespace parac::solver_qbf {
class Parser;

class DepQBFHandle : public GenericSolverHandle {
  public:
  DepQBFHandle(const Parser& parser);
  virtual ~DepQBFHandle();

  virtual void assumeCube(const CubeIteratorRange& cube);
  virtual parac_status solve();
  virtual void terminate();

  private:
  const Parser& m_parser;
  std::unique_ptr<QDPLL, void (*)(QDPLL*)> m_qdpll;

  volatile bool m_terminated = false;

  void addParsedQDIMACS();
};
}
