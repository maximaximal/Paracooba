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
  PortfolioQBFHandle::Event event;
  bool previousWasAssume = false;

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
    return StateEnd;
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

  auto& handles = *m_handles;
  for(uintptr_t i = 0; i < solverHandleFactories.size(); ++i) {
    Handle& h = handles[i];
    h.i = i;
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

  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Begin waiting for all {} handles to be ready...",
            solverHandleFactories.size());

  std::unique_lock lock(m_eventsMutex);
  waitForAllToBeReady(lock);
  m_name = computeName();

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "All handles initialized, {} ready.", m_name);

  m_parser = nullptr;
}
PortfolioQBFHandle::~PortfolioQBFHandle() {
  m_handleRunning = false;
  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "PortfolioQBFHandle beginning to destruct.");
  terminate();
  pushEventAndWait(Event(Event::Destruct));
  m_handles->clear();
  parac_log(PARAC_SOLVER, PARAC_DEBUG, "Destructed PortfolioQBFHandle!");
}

void
PortfolioQBFHandle::assumeCube(const CubeIteratorRange& cube) {
  m_working = true;
  m_terminating = false;

  std::unique_lock eventsLock(m_eventsMutex);
  pushEvent(Event(Event::Assume, cube), eventsLock);
}
parac_status
PortfolioQBFHandle::solve() {
  m_terminating = false;
  m_solveResult = PARAC_ABORTED;
  pushEventAndWait(Event(Event::Solve));
  parac_status s = m_solveResult;
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Portfolio Solver {} finished with result {} from solver id {} "
            "with name {}!",
            m_name,
            s,
            m_solveResultProducerIndex,
            (*m_handles)[m_solveResultProducerIndex].solver_handle->name());

  m_terminating = false;
  m_working = false;
  return m_solveResult;
}
void
PortfolioQBFHandle::terminate() {
  // Only do the termination stuff once!
  if(!m_terminating) {
    m_terminating = true;
    {
      std::unique_lock lock(m_eventsMutex);
      pushEvent(Event(Event::Terminate), lock);
    }
    for(auto& h : *m_handles) {
      if(h.solver_handle) {
        h.solver_handle->terminate();
      }
    }
  }
}

int64_t
PortfolioQBFHandle::pushEvent(const Event& e,
                              std::unique_lock<std::mutex>& lock) {
  m_events.emplace(e);
  int64_t n = m_eventCount++;
  m_events.back().eventNumber = n;
  m_eventsConditionVariable.notify_all();
  return n;
}
void
PortfolioQBFHandle::pushEventAndWait(const Event& e) {
  std::unique_lock lock(m_eventsMutex);
  int64_t n = pushEvent(e, lock);

  while(m_mostRecentHandledEvent < n) {
    m_eventsReverseConditionVariable.wait(lock);
  }
}

bool
PortfolioQBFHandle::popEvent(Event& tgt) {
  auto oldEvNum = tgt.eventNumber;

  std::unique_lock lock(m_eventsMutex);

  if(++m_handlesThatProcessedCurrentEvent == m_handles->size()) {
    m_handlesThatProcessedCurrentEvent = 0;
    m_mostRecentHandledEvent = tgt.eventNumber;
    m_eventsReverseConditionVariable.notify_all();
  }

  while(true) {
    if(!m_events.empty()) {
      const auto& front = m_events.front();
      if(front.eventNumber > oldEvNum) {
        tgt = front;

        if(++m_handlesThatReceivedCurrentEvent == m_handles->size()) {
          m_handlesThatReceivedCurrentEvent = 0;
          m_events.pop();

          // Others that have been waiting for this moment must now be notified
          // again.
          m_eventsConditionVariable.notify_all();
        }

        return tgt.type != Event::Destruct;
      }
    }

    m_eventsConditionVariable.wait(lock);
  }

  assert(m_events.empty());

  return false;
}

PortfolioQBFHandle::Handle::State
PortfolioQBFHandle::Handle::StateWaitFunc(PortfolioQBFHandle& self) {
  parac_log(
    PARAC_SOLVER,
    PARAC_DEBUG,
    "Inner PortfolioQBFHandle with id {} and solver {} now in state Wait.",
    i,
    solver_handle->name());

  while(self.popEvent(event)) {
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Inner PortfolioQBFHandle with id {} and solver {} received "
              "event of type {} in state Wait.",
              i,
              solver_handle->name(),
              Event::typeToStr(event.type));

    switch(event.type) {
      case Event::Assume:
        previousWasAssume = true;
        return StateAssume;
      case Event::Solve:
        // If the previous command was to assume, the assume state already
        // solves now. This is to stop issues with aborts putting the solver
        // into an invalid state when the terminate comes before the solver
        // could start solving.
        if(previousWasAssume) {
          previousWasAssume = false;
          continue;
        }
        return StateSolve;
      case Event::Terminate:
        previousWasAssume = false;
        continue;
      case Event::Destruct:
      case Event::Undefined:
        previousWasAssume = false;
        return StateEnd;
    }
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
    CubeIteratorRange(event.cube.begin(), event.cube.end()));
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

  if(s != PARAC_SAT && s != PARAC_UNSAT && s != PARAC_ABORTED) {
    parac_log(PARAC_SOLVER,
              PARAC_LOCALERROR,
              "Inner PortfolioQBFHandle with id {} and solver {} returned "
              "unsupported status {}! Ignoring this result.",
              i,
              solver_handle->name(),
              s);
  } else {
    std::unique_lock lock(self.m_solveResultMutex, std::try_to_lock);

    if(lock.owns_lock() && self.m_eventAlreadySolved < event.eventNumber) {
      self.m_eventAlreadySolved = event.eventNumber;

      self.terminateAllBut(*this);
      parac_log(PARAC_SOLVER,
                PARAC_DEBUG,
                "Inner PortfolioQBFHandle with id {} and solver {} to finished "
                "solving with result {}!",
                i,
                solver_handle->name(),
                s);
      self.m_solveResult = s;
      self.m_solveResultProducerIndex = i;
    }
  }

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

    parac_log(
      PARAC_SOLVER,
      PARAC_DEBUG,
      "Inner PortfolioQBFHandle with id {} and solver {} exiting. Event: {}",
      h.i,
      h.solver_handle->name(),
      Event::typeToStr(h.event.type));

    if(h.event.type == Event::Destruct) {
      std::unique_lock lock(m_eventsMutex);
      m_mostRecentHandledEvent = m_eventCount - 1;
      m_eventsReverseConditionVariable.notify_all();
    }
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
PortfolioQBFHandle::waitForAllToBeReady(std::unique_lock<std::mutex>& lock) {
  if(!m_handleRunning || m_terminating) {
    m_readyHandles = 0;
    return;
  }

  while(m_readyHandles != m_handles->size() && m_handleRunning &&
        !m_terminating) {
    m_eventsConditionVariable.wait(lock);
  }

  m_readyHandles = 0;
}
void
PortfolioQBFHandle::beReady() {
  std::unique_lock lock(m_eventsMutex);
  ++m_readyHandles;
  m_eventsConditionVariable.notify_all();
}
void
PortfolioQBFHandle::terminateAllBut(Handle& ignore) {
  for(auto& h : *m_handles) {
    if(&h == &ignore)
      continue;

    h.solver_handle->terminate();
  }
}
}
