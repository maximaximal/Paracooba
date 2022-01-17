#pragma once

#include <condition_variable>
#include <functional>

#include "generic_qbf_handle.hpp"

struct parac_module;
struct parac_thread_registry;
struct parac_thread_registry_handle;

namespace parac::solver_qbf {
class Parser;

class PortfolioQBFHandle : public GenericSolverHandle {
  public:
  using SolverHandle = std::unique_ptr<GenericSolverHandle>;
  using SolverHandleFactory = std::function<SolverHandle(const Parser&)>;
  using SolverHandleFactoryVector = std::vector<SolverHandleFactory>;

  PortfolioQBFHandle(parac_module& mod,
                     const Parser& parser,
                     SolverHandleFactoryVector& handles);
  ~PortfolioQBFHandle();

  virtual void assumeCube(const CubeIteratorRange& cube) override;
  virtual parac_status solve() override;
  virtual void terminate() override;
  virtual const char* name() const override { return m_name.c_str(); };

  private:
  struct Handle;
  using Handles = std::unique_ptr<std::vector<Handle>>;

  std::string m_name;
  parac_module& m_mod;
  const Parser* m_parser = nullptr;

  std::condition_variable m_cond;
  std::condition_variable m_readynessCond;

  int m_readyHandles = 0;

  std::mutex m_waitMutex;
  const CubeIteratorRange* volatile m_cubeIteratorRange = nullptr;

  std::mutex m_solveResultMutex;
  volatile parac_status m_solveResult;

  const Handles m_handles;
  volatile bool m_handleRunning = true;
  volatile bool m_terminating = false, m_globallyTerminating = false;
  volatile bool m_working = false;

  volatile bool m_setCubeIteratorRange = false;
  volatile bool m_setStartSolve = false;

  parac_status handleRun(Handle& h);

  Cube m_cube;

  std::string computeName();
  void resetReadyness();
  void waitForAllToBeReady(std::unique_lock<std::mutex>& lock);
  void beReady();
  void terminateAllBut(Handle& h);
};
}
