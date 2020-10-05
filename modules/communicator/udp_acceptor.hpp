#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <paracooba/common/status.h>

namespace parac::communicator {
class Service;

class UDPAcceptor {
  public:
  UDPAcceptor(const std::string& listenAddressStr, uint16_t listenPort);
  ~UDPAcceptor();

  parac_status start(Service& service);

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  std::string m_listenAddressStr;
  uint16_t m_listenPort;
};
}
