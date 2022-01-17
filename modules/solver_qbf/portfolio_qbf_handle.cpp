#include "portfolio_qbf_handle.hpp"
#include "generic_qbf_handle.hpp"
#include "paracooba/common/status.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <mutex>
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

  enum State { StateWait, StateAssume, StateSolve, StateEnd };

  State StateWaitFunc(PortfolioQBFHandle& self);
  State StateAssumeFunc(PortfolioQBFHandle& self);
  State StateSolveFunc(PortfolioQBFHandle& self);

  State executeState(State s, PortfolioQBFHandle& self) {
    switch(s) {
      case StateWait:
        return StateWaitFunc(self);
      case StateAssume:
        return StateAssumeFunc(self);
      case StateSolve:
        return StateSolveFunc(self);
      case StateEnd:
        return StateEnd;
    }
  }

  ~Handle() { parac_thread_registry_wait_for_exit_of_thread(&thread_handle); }
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

  std::unique_lock lock(m_waitMutex);
  waitForAllToBeReady(lock);
  m_name = computeName();

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "All handles initialized, {} ready.", m_name);

  m_parser = nullptr;
}
PortfolioQBFHandle::~PortfolioQBFHandle() {
  m_handleRunning = false;
  parac_log(PARAC_SOLVER,
            PARAC_LOCALWARNING,
            "PortfolioQBFHandle beginning to destruct.");
  terminate();
  parac_log(PARAC_SOLVER,
            PARAC_LOCALWARNING,
            "PortfolioQBFHandle beginning to destruct. After terminate");

  // Don't destruct while working!
  if(m_working) {
    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "Destructing PortfolioQBFHandle, waiting for work to end.");
    while(m_working) {
    };
  }

  if(std::any_of(m_handles->begin(), m_handles->end(), [](const auto& h) {
       return h.thread_handle.running;
     })) {
    parac_log(PARAC_SOLVER,
              PARAC_LOCALWARNING,
              "Some working handles still exist for {}! Repeating condition "
              "variable notification until they stop.",
              m_name);
    do {
      m_cond.notify_all();
    } while(std::any_of(m_handles->begin(),
                        m_handles->end(),
                        [](const auto& h) { return h.thread_handle.running; }));
  } else {
    parac_log(PARAC_SOLVER,
              PARAC_LOCALWARNING,
              "No running handles in {}! Iterating.",
              m_name);
    for(const auto& h : *m_handles) {
      parac_log(PARAC_SOLVER,
                PARAC_LOCALWARNING,
                "Handle {} Active {}",
                h.i,
                h.thread_handle.running);
    }
  }

  m_handles->clear();
  parac_log(PARAC_SOLVER, PARAC_DEBUG, "Destructed PortfolioQBFHandle!");
}

void
PortfolioQBFHandle::assumeCube(const CubeIteratorRange& cube) {
  if(m_terminating || m_globallyTerminating)
    return;
  std::unique_lock lock(m_waitMutex);
  m_cube.assign(cube.begin(), cube.end());
  m_working = true;
  resetReadyness();
  m_setCubeIteratorRange = true;
  parac_log(PARAC_SOLVER,
            PARAC_LOCALWARNING,
            "Wanna Assume",
            m_setCubeIteratorRange);
  m_cond.notify_all();
}
parac_status
PortfolioQBFHandle::solve() {
  if(m_terminating || m_globallyTerminating)
    return PARAC_ABORTED;
  std::unique_lock lock(m_waitMutex);
  parac_log(PARAC_SOLVER,
            PARAC_LOCALWARNING,
            "Wanna Solve! Assumed: {}!",
            m_setCubeIteratorRange);
  if(!m_setCubeIteratorRange) {
    m_solveResult = PARAC_ABORTED;
    m_working = true;
    resetReadyness();
    m_setStartSolve = true;
    m_cond.notify_all();
  }
  waitForAllToBeReady(lock);
  m_setCubeIteratorRange = false;
  m_working = false;
  m_terminating = false;
  m_setStartSolve = false;

  parac_status s = m_solveResult;
  parac_log(
    PARAC_SOLVER, PARAC_LOCALWARNING, "Solver finished with result {}!", s);

  if(m_globallyTerminating) {
    m_working = false;
    return PARAC_ABORTED;
  }
  m_terminating = false;
  m_working = false;
  return m_solveResult;
}
void
PortfolioQBFHandle::terminate() {
  m_terminating = true;
  for(auto& h : *m_handles) {
    if(h.solver_handle) {
      h.solver_handle->terminate();
    }
  }
  m_cond.notify_all();
  m_readynessCond.notify_all();
}

PortfolioQBFHandle::Handle::State
PortfolioQBFHandle::Handle::StateWaitFunc(PortfolioQBFHandle& self) {
  parac_log(
    PARAC_SOLVER,
    PARAC_DEBUG,
    "Inner PortfolioQBFHandle with id {} and solver {} now in state Wait.",
    i,
    solver_handle->name());

  std::unique_lock lock(self.m_waitMutex);

  while(self.m_handleRunning) {
    if(self.m_setCubeIteratorRange)
      return StateAssume;
    if(self.m_setStartSolve)
      return StateSolve;

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Inner PortfolioQBFHandle with id {} and solver {} now in state "
              "Wait. Trigger Wait.",
              i,
              solver_handle->name());

    self.m_cond.wait(lock);
  }

  return StateEnd;
}

PortfolioQBFHandle::Handle::State
PortfolioQBFHandle::Handle::StateAssumeFunc(PortfolioQBFHandle& self) {
  parac_log(
    PARAC_SOLVER,
    PARAC_DEBUG,
    "Inner PortfolioQBFHandle with id {} and solver {} now in state Assume.",
    i,
    solver_handle->name());
  solver_handle->assumeCube(
    CubeIteratorRange(self.m_cube.begin(), self.m_cube.end()));
  return StateSolve;
}

PortfolioQBFHandle::Handle::State
PortfolioQBFHandle::Handle::StateSolveFunc(PortfolioQBFHandle& self) {
  parac_log(
    PARAC_SOLVER,
    PARAC_DEBUG,
    "Inner PortfolioQBFHandle with id {} and solver {} now in state Solve.",
    i,
    solver_handle->name());

  parac_status s = solver_handle->solve();
  std::unique_lock lock(self.m_solveResultMutex, std::try_to_lock);

  if(lock.owns_lock()) {
    if(!self.m_terminating) {
      self.terminateAllBut(*this);
    }
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Inner PortfolioQBFHandle with id {} and solver {} to finished "
              "solving with result {}!",
              i,
              solver_handle->name(),
              s);
    self.m_solveResult = s;
  }

  self.beReady();
  return StateWait;
}

parac_status
PortfolioQBFHandle::handleRun(Handle& h) {
  try {
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "PortfolioQBFHandle with id {} started! Producing "
              "solver from factory.",
              h.i);

    assert(m_parser);

    h.solver_handle = h.solver_handle_factory(*m_parser);

    parac_log(
      PARAC_SOLVER,
      PARAC_DEBUG,
      "Inner PortfolioQBFHandle with id {} and solver {} produced solver! "
      "Ready for work.",
      h.i,
      h.solver_handle->name());

    beReady();

    Handle::State s = Handle::StateWait;

    while(s != Handle::StateEnd) {
      s = h.executeState(s, *this);
    }

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Inner PortfolioQBFHandle with id {} and solver {} exiting.",
              h.i,
              h.solver_handle->name());
  } catch(...) {
  }

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
PortfolioQBFHandle::waitForAllToBeReady(std::unique_lock<std::mutex>& lock) {
  if(!m_handleRunning || m_terminating) {
    m_readyHandles = 0;
    return;
  }

  while(m_readyHandles != m_handles->size() && m_handleRunning &&
        !m_terminating) {
    m_readynessCond.wait(lock);
  }

  m_readyHandles = 0;
}
void
PortfolioQBFHandle::beReady() {
  std::unique_lock lock(m_waitMutex);
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
