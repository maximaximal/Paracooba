#pragma once

#include <string>

#include <cereal/types/string.hpp>

#include <paracooba/common/types.h>

namespace parac::communicator {
struct UDPAnnouncement {
  UDPAnnouncement() = default;
  UDPAnnouncement(parac_id id, const std::string& tcpConnectionString)
    : id(id)
    , tcpConnectionString(tcpConnectionString) {}
  ~UDPAnnouncement() = default;

  parac_id id = 0;
  std::string tcpConnectionString;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(id, tcpConnectionString);
  }
};
}
