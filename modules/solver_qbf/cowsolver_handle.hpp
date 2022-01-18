#pragma once

#include "generic_qbf_handle.hpp"
#include "solver_qbf_config.hpp"
#include <memory>
#include <vector>

struct cowipasir_solver;

namespace parac::solver_qbf {
class Parser;

class CowSolverHandle : public GenericSolverHandle {
  public:
  CowSolverHandle(const Parser& parser,
                  std::string path,
                  const char** argv,
                  const char** envp,
                  int prefixdepth,
                  std::optional<std::string> name,
                  std::optional<std::string> SAT_regex = std::nullopt,
                  std::optional<std::string> UNSAT_regex = std::nullopt);
  CowSolverHandle(const Parser& parser,
                  const SolverConfig& config,
                  const SolverConfig::CowSolver& cowSolver)
    : CowSolverHandle(parser,
                      cowSolver.path,
                      cowSolver.argv_cstyle.get(),
                      cowSolver.envp_cstyle.get(),
                      config.fullyRealizedTreeDepth(),
                      "CowSolver {" + cowSolver.name() + "}",
                      cowSolver.SAT_regex,
                      cowSolver.UNSAT_regex) {}

  virtual ~CowSolverHandle();

  virtual void assumeCube(const CubeIteratorRange& cube) override;
  virtual parac_status solve() override;
  virtual void terminate() override;
  virtual const char* name() const override { return m_name.c_str(); };

  private:
  std::string m_name;

  volatile bool m_terminated = false;

  std::unique_ptr<cowipasir_solver, void (*)(cowipasir_solver*)> m_cowipasir;
};
}
