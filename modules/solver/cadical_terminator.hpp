#pragma once

#include <atomic>

#include <cadical/cadical.hpp>

namespace parac::solver {
class CaDiCaLTerminator : public CaDiCaL::Terminator {
  public:
  explicit CaDiCaLTerminator(volatile bool* terminated = nullptr)
    : m_terminated(terminated) {}
  void setTerminatedPointer(volatile bool* terminated) {
    m_terminated = terminated;
  }

  virtual ~CaDiCaLTerminator() {}

  virtual bool terminate() override { return *this; }

  operator bool() const {
    return m_locallyTerminated || (m_terminated && *m_terminated);
  }
  void terminateLocally(bool terminate = true) {
    m_locallyTerminated = terminate;
  }

  private:
  volatile bool* m_terminated;
  std::atomic_bool m_locallyTerminated = false;
};
}
