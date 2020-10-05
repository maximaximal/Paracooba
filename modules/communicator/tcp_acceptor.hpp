#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <paracooba/common/status.h>

namespace boost::system {
class error_code;
}

namespace parac::communicator {
class Service;

class TCPAcceptor {
  public:
  TCPAcceptor(const std::string& listenAddressStr, uint16_t listenPort);
  ~TCPAcceptor();

  parac_status start(Service& service);

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  void loop(const boost::system::error_code& ec);

  std::string m_listenAddressStr;
  uint16_t m_listenPort;
};
}
