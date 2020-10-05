#pragma once

#include <optional>
#include <string>

namespace boost::asio::ip {
class address;
}

namespace parac::communicator {
std::optional<boost::asio::ip::address>
ParseIPAddress(const std::string& addressStr);
}
