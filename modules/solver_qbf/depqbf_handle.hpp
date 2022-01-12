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

  virtual void assumeCube(const CubeIteratorRange& cube) override;
  virtual parac_status solve() override;
  virtual void terminate() override;
  virtual const char* name() const override { return "DepQBFHandle"; };

  private:
  const Parser& m_parser;
  std::unique_ptr<QDPLL, void (*)(QDPLL*)> m_qdpll;

  volatile bool m_terminated = false;
  volatile bool m_running = false;

  void addParsedQDIMACS();
};
}
