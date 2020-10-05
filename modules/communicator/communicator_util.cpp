#include "communicator_util.hpp"

#include <boost/asio/ip/address.hpp>

#include <paracooba/common/log.h>

namespace parac::communicator {
std::optional<boost::asio::ip::address>
ParseIPAddress(const std::string& addressStr) {
  boost::system::error_code err;
  auto address = boost::asio::ip::address::from_string(addressStr, err);
  if(err) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Could not parse given IP Address \"{}\"! Error: {}");
    return std::nullopt;
  }
  return address;
}
}
