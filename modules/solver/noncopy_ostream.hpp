#pragma once

#include <cassert>
#include <sstream>

namespace parac::solver {
class NoncopyStringbuf : public std::stringbuf {
  public:
  char* eback() { return std::streambuf::eback(); }
  char* pptr() { return std::streambuf::pptr(); }
};

class NoncopyOStringstream : public std::ostringstream {
  public:
  NoncopyOStringstream() {
    // replace buffer
    std::basic_ostream<char>::rdbuf(&m_buf);
  }
  char* ptr() {
    assert(tellp() == (m_buf.pptr() - m_buf.eback()));
    return m_buf.eback();
  }

  private:
  NoncopyStringbuf m_buf;
};
}
