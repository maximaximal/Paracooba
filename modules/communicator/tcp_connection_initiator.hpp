#pragma once

#include "paracooba/common/status.h"
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

namespace boost::system {
class error_code;
}

namespace parac::communicator {
class Service;
class TCPConnection;

class TCPConnectionInitiator {
  public:
  using Callback =
    std::function<void(std::optional<TCPConnection>, parac_status)>;

  TCPConnectionInitiator(Service& service,
                         const std::string& host,
                         Callback cb = nullptr,
                         int connectionTry = 0,
                         bool delayed = false);
  TCPConnectionInitiator(Service& service,
                         boost::asio::ip::tcp::endpoint endpoint,
                         Callback cb = nullptr,
                         int connectionTry = 0,
                         bool delayed = false);
  TCPConnectionInitiator(const TCPConnectionInitiator& initiator);
  ~TCPConnectionInitiator();

  void run();

  void try_connecting_to_host(
    const ::boost::system::error_code& ec,
    ::boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
  void try_connecting_to_endpoint(const ::boost::system::error_code& ec);

  bool isEndpointSameAsLocalTCPAcceptor(
    const boost::asio::ip::tcp::endpoint& e) const;

  private:
  struct State;
  std::shared_ptr<State> m_state;

  using DelayedRunFunc =
    std::function<void(TCPConnectionInitiator&& initiator)>;

  void retryConnection();
};
}
