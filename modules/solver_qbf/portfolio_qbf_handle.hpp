#pragma once

#include <condition_variable>
#include <functional>
#include <queue>

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

  struct Event {
    enum Type {
      Assume,
      Solve,
      Terminate,
      Destruct,
      Undefined,
    };

    static inline constexpr bool typeHasCube(Type t) {
      switch(t) {
        case Assume:
          return true;
        default:
          return false;
      }
    }
    static inline constexpr const char* typeToStr(Type t) {
      switch(t) {
        case Assume:
          return "Assume";
        case Solve:
          return "Solve";
        case Terminate:
          return "Terminate";
        case Destruct:
          return "Destruct";
        case Undefined:
          return "Undefined";
      }
    }

    int64_t eventNumber = -1;

    Type type = Undefined;

    /// Only valid if Assume or Solve Event
    Cube cube;

    Event() noexcept {};
    Event(Type t) noexcept
      : type(t) {}
    Event(Type t, const CubeIteratorRange& cubeItRange)
      : type(t)
      , cube(cubeItRange.begin(), cubeItRange.end()) {}

    Event& operator=(const Event& o) {
      eventNumber = o.eventNumber;
      type = o.type;
      if(typeHasCube(o.type))
        cube = o.cube;
      return *this;
    }
  };

  /** @brief Pops a new event from the event queue.
   * @return true to continue, false to exit the worker thread.
   */
  bool popEvent(Event& tgt);
  int64_t pushEvent(const Event& e, std::unique_lock<std::mutex>& lock);
  void pushEventAndWait(const Event& e);
  std::mutex m_eventsMutex;
  std::queue<Event> m_events;
  std::condition_variable m_eventsConditionVariable;
  std::condition_variable m_eventsReverseConditionVariable;
  size_t m_handlesThatReceivedCurrentEvent = 0;
  size_t m_handlesThatProcessedCurrentEvent = 0;

  std::string m_name;
  parac_module& m_mod;
  const Parser* m_parser = nullptr;

  int64_t m_eventCount = 0;
  int64_t m_mostRecentHandledEvent = -1;

  size_t m_readyHandles = 0;
  volatile int64_t m_eventAlreadySolved = -1;

  std::mutex m_solveResultMutex;
  volatile parac_status m_solveResult;
  volatile size_t m_solveResultProducerIndex;

  const Handles m_handles;
  volatile bool m_handleRunning = true;
  volatile bool m_terminating = false;
  volatile bool m_working = false;

  parac_status handleRun(Handle& h);

  std::string computeName();
  void waitForAllToBeReady(std::unique_lock<std::mutex>& lock);
  void beReady();
  void terminateAllBut(Handle& h);
};
}
