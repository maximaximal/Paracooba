#pragma once

#include "generic_qbf_handle.hpp"
#include "solver_qbf_config.hpp"
#include <memory>
#include <vector>

struct quapi_solver;

namespace parac::solver_qbf {
class Parser;

class QuapiSolverHandle : public GenericSolverHandle {
  public:
  QuapiSolverHandle(const Parser& parser,
                    std::string path,
                    const char** argv,
                    const char** envp,
                    int prefixdepth,
                    std::optional<std::string> name,
                    std::optional<std::string> SAT_regex = std::nullopt,
                    std::optional<std::string> UNSAT_regex = std::nullopt);
  QuapiSolverHandle(const Parser& parser,
                    const SolverConfig& config,
                    const SolverConfig::QuapiSolver& quapiSolver)
    : QuapiSolverHandle(parser,
                        quapiSolver.path,
                        quapiSolver.argv_cstyle.get(),
                        quapiSolver.envp_cstyle.get(),
                        config.fullyRealizedTreeDepth(),
                        "QuapiSolver {" + quapiSolver.name() + "}",
                        quapiSolver.SAT_regex,
                        quapiSolver.UNSAT_regex) {}

  virtual ~QuapiSolverHandle();

  virtual void assumeCube(const CubeIteratorRange& cube) override;
  virtual parac_status solve() override;
  virtual void terminate() override;
  virtual const char* name() const override { return m_name.c_str(); };

  private:
  std::string m_name;

  volatile bool m_terminated = false;

  std::unique_ptr<quapi_solver, void (*)(quapi_solver*)> m_quapi;
};
}
