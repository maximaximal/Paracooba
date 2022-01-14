#include "portfolio_qbf_handle.hpp"
#include "generic_qbf_handle.hpp"
#include "paracooba/common/status.h"

#include <cassert>
#include <cstdint>
#include <sstream>

#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>
#include <paracooba/common/types.h>
#include <paracooba/module.h>

/** General flow of this handle:
 *
 * 1. Start handle workers.
 * 2. In the workers, wait for condition variable that starts a solving run.
 * 3. Once solving is done, main thread waits for condition variable from any
 *    other solver.
 * 4. Once end is received, others are stopped.
 * 5. Reset to original state.
 * 6. Give result to caller.
 */

namespace parac::solver_qbf {
struct PortfolioQBFHandle::Handle {
  parac_thread_registry_handle thread_handle;
  SolverHandleFactory solver_handle_factory;
  SolverHandle solver_handle;
  size_t i;
};

PortfolioQBFHandle::PortfolioQBFHandle(
  parac_module& mod,
  const Parser& parser,
  SolverHandleFactoryVector& solverHandleFactories)
  : m_mod(mod)
  , m_parser(&parser)
  , m_handles(std::make_unique<std::vector<PortfolioQBFHandle::Handle>>(
      solverHandleFactories.size())) {
  assert(mod.handle);
  parac_handle& handle = *mod.handle;

  resetReadyness();

  auto& handles = *m_handles;
  for(uintptr_t i = 0; i < solverHandleFactories.size(); ++i) {
    Handle& h = handles[i];
    h.thread_handle.userdata = this;
    h.solver_handle_factory = std::move(solverHandleFactories[i]);

    parac_thread_registry_create(
      handle.thread_registry,
      handle.modules[PARAC_MOD_SOLVER],
      [](parac_thread_registry_handle* threadHandle) -> int {
        PortfolioQBFHandle* self =
          static_cast<PortfolioQBFHandle*>(threadHandle->userdata);
        Handle* h = reinterpret_cast<Handle*>(threadHandle);
        assert(self);
        assert(h);
        return self->handleRun(*h);
      },
      &h.thread_handle);
  }

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Begin waiting for all handles to be ready...");

  waitForAllToBeReady();
  m_name = computeName();

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "All handles initialized, {} ready.", m_name);

  m_parser = nullptr;
}
PortfolioQBFHandle::~PortfolioQBFHandle() {
  m_handleRunning = false;
  terminate();

  // Don't destruct while working!
  if(m_working) {
    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "Destructing PortfolioQBFHandle, waiting for work to end.");
    while(m_working) {
    };
  }
  parac_log(PARAC_SOLVER, PARAC_DEBUG, "Destructed PortfolioQBFHandle!");
}

void
PortfolioQBFHandle::assumeCube(const CubeIteratorRange& cube) {
  if(m_terminating || m_globallyTerminating)
    return;
  m_working = true;
  m_cubeIteratorRange = &cube;
  resetReadyness();
  m_cond.notify_all();
  waitForAllToBeReady();
  m_cubeIteratorRange = nullptr;
  m_working = false;
  m_terminating = false;
  m_globallyTerminating = false;
}
parac_status
PortfolioQBFHandle::solve() {
  if(m_terminating || m_globallyTerminating)
    return PARAC_ABORTED;
  m_working = true;
  m_solveResult = PARAC_ABORTED;
  resetReadyness();
  m_cond.notify_all();
  waitForAllToBeReady();
  if(m_globallyTerminating) {
    m_terminating = false;
    m_globallyTerminating = false;
    return PARAC_ABORTED;
  }
  m_terminating = false;
  m_globallyTerminating = false;
  std::unique_lock lock(m_solveResultMutex);
  m_working = false;
  return m_solveResult;
}
void
PortfolioQBFHandle::terminate() {
  m_terminating = true;
  m_globallyTerminating = true;
  for(auto& h : *m_handles) {
    if(h.solver_handle) {
      h.solver_handle->terminate();
    }
  }
  m_cond.notify_all();
  m_readynessCond.notify_all();
}

parac_status
PortfolioQBFHandle::handleRun(Handle& h) {
  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "PortfolioQBFHandle with id {} started! Producing "
            "solver from factory.",
            h.i);

  assert(m_parser);

  h.solver_handle = h.solver_handle_factory(*m_parser);

  beReady();

  parac_log(
    PARAC_SOLVER,
    PARAC_DEBUG,
    "Inner PortfolioQBFHandle with id {} and solver {} produced solver! "
    "Ready for work.",
    h.i,
    h.solver_handle->name());

  beReady();

  while(m_handleRunning) {
    {
      std::unique_lock lock(m_waitMutex);
      m_cond.wait(lock);
    }

    // If already terminating here, the solver handle should directly be ended!
    // The outside world doesn't know about this internal state machine. Only if
    // not already working though.
    if(!m_handleRunning || (m_terminating && !m_working)) {
      beReady();
      return PARAC_ABORTED;
    }

    // Some other thread woke up early / this thread slept longer and the whole
    // solver handle was already aborted! Back to running.
    if(m_terminating) {
      beReady();
      continue;
    }

    if(m_cubeIteratorRange) {
      // First wait for call to solve after applying assumptions!
      h.solver_handle->assumeCube(*m_cubeIteratorRange);

      {
        beReady();
        std::unique_lock lock(m_waitMutex);
        m_cond.wait(lock);
      }
    }

    if(!m_handleRunning)
      return PARAC_ABORTED;
    if(m_terminating)
      continue;

    parac_log(
      PARAC_SOLVER,
      PARAC_TRACE,
      "Inner PortfolioQBFHandle with id {} and solver {} to start solving!",
      h.i,
      h.solver_handle->name());

    parac_status s = h.solver_handle->solve();
    if(!m_terminating) {
      parac_log(PARAC_SOLVER,
                PARAC_DEBUG,
                "Inner PortfolioQBFHandle with id {} and solver {} to finished "
                "solving with result {}!",
                h.i,
                h.solver_handle->name(),
                s);
      terminateAllBut(h);
      std::unique_lock lock(m_solveResultMutex);
      m_solveResult = s;
      beReady();
    } else {
      parac_log(
        PARAC_SOLVER,
        PARAC_DEBUG,
        "Inner PortfolioQBFHandle with id {} and solver {} was terminated.",
        h.i,
        h.solver_handle->name());
      beReady();
    }
  }

  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Inner PortfolioQBFHandle with id {} and solver {} exiting.",
            h.i,
            h.solver_handle->name());

  return PARAC_OK;
}

std::string
PortfolioQBFHandle::computeName() {
  std::stringstream name;
  name << "PortfolioQBFHandle { ";
  for(const auto& h : *m_handles) {
    name << h.solver_handle->name() << " ";
  }
  name << "}";
  return name.str();
}
void
PortfolioQBFHandle::resetReadyness() {
  m_readyHandles = 0;
  m_terminating = false;
  m_globallyTerminating = false;
}
void
PortfolioQBFHandle::waitForAllToBeReady() {
  if(!m_handleRunning || m_terminating)
    return;

  std::unique_lock lock(m_readyHandlesMutex);
  while(m_readyHandles != m_handles->size() && m_handleRunning &&
        !m_terminating) {
    m_readynessCond.wait(lock);

    if(m_readyHandles != m_handles->size()) {
      parac_log(PARAC_SOLVER,
                PARAC_DEBUG,
                "waitForAllToBeReady readyHandles: {} m_handles->size(): {} "
                "m_terminating: {} m_handleRunning: {} globalTerm: {}",
                m_readyHandles,
                m_handles->size(),
                m_terminating,
                m_handleRunning,
                m_globallyTerminating);
    }
  }
}
void
PortfolioQBFHandle::beReady() {
  std::unique_lock lock(m_readyHandlesMutex);
  ++m_readyHandles;
  m_readynessCond.notify_all();
}
void
PortfolioQBFHandle::terminateAllBut(Handle& ignore) {
  m_terminating = true;
  for(auto& h : *m_handles) {
    if(&h == &ignore)
      continue;

    h.solver_handle->terminate();
  }
}
}
