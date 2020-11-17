#pragma once

#include <cadical/cadical.hpp>

namespace parac::solver {
class CaDiCaLTerminator : public CaDiCaL::Terminator {
  public:
  explicit CaDiCaLTerminator(bool& terminated)
    : m_terminated(terminated) {}
  virtual ~CaDiCaLTerminator() {}

  virtual bool terminate() { return m_terminated; }

  bool& stopRef() const { return m_terminated; }

  private:
  bool& m_terminated;
};
}
