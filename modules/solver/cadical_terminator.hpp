#pragma once

#include <cadical/cadical.hpp>

namespace parac::solver {
class CaDiCaLTerminator : public CaDiCaL::Terminator {
  public:
  explicit CaDiCaLTerminator(bool& terminated)
    : m_terminated(terminated) {}
  virtual ~CaDiCaLTerminator() {}

  virtual bool terminate() { return m_terminated || m_locallyTerminated; }

  bool& stopRef() const { return m_terminated; }
  void terminateLocally(bool terminate = true) {
    m_locallyTerminated = terminate;
  }

  private:
  bool& m_terminated;
  bool m_locallyTerminated = false;
};
}
