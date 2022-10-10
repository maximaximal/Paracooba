#pragma once

#include "generic_sat_handle.hpp"
#include "solver_config.hpp"
#include <memory>
#include <vector>

struct quapi_solver;

namespace parac::solver {
class Parser;
class CaDiCaLHandle;

class QuapiSolverHandle : public GenericSolverHandle {
  public:
  QuapiSolverHandle(CaDiCaLHandle& cadicalHandle,
                    std::string path,
                    const char** argv,
                    const char** envp,
                    int prefixdepth,
                    std::optional<std::string> name,
                    std::optional<std::string> SAT_regex = std::nullopt,
                    std::optional<std::string> UNSAT_regex = std::nullopt);
  QuapiSolverHandle(CaDiCaLHandle& cadicalHandle,
                    const SolverConfig& config,
                    const SolverConfig::QuapiSolver& quapiSolver)
    : QuapiSolverHandle(cadicalHandle,
                        quapiSolver.path,
                        quapiSolver.argv_cstyle.get(),
                        quapiSolver.envp_cstyle.get(),
                        config.InitialMinimalCubeDepth(),
                        "QuapiSolver {" + quapiSolver.name() + "}",
                        quapiSolver.SAT_regex,
                        quapiSolver.UNSAT_regex) {}

  virtual ~QuapiSolverHandle();

  virtual void assumeCube(const CubeIteratorRange& cube) override;
  virtual parac_status solve() override;
  virtual void terminate() override;
  virtual const char* name() const override { return m_name.c_str(); };

  private:
  std::unique_ptr<quapi_solver, void (*)(quapi_solver*)> m_quapi;
  std::string m_name;
  volatile bool m_terminated = false;
  int m_prefixdepth;
};
}
