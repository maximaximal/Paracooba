#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <paracooba/common/status.h>

namespace boost::system {
struct error_code;
}

namespace parac::communicator {
class Service;

class UDPAcceptor {
  public:
  UDPAcceptor(const std::string& listenAddressStr, uint16_t listenPort);
  ~UDPAcceptor();

  parac_status start(Service& service);

  void startAnnouncements(const std::string& connectionString,
                          int32_t announcementIntervalMS);

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  void readHandler(const boost::system::error_code& ec, size_t bytes);
  void accept();

  void announce();
  void announceAndScheduleNext(const boost::system::error_code& ec);

  std::string m_listenAddressStr;
  uint16_t m_listenPort;

  int32_t m_announcementIntervalMS;
};
}
