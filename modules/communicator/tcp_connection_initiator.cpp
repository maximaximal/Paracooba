#include <variant>

#include "paracooba/common/log.h"
#include "service.hpp"
#include "tcp_connection_initiator.hpp"

#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>

namespace parac::communicator {
struct TCPConnectionInitiator::State {
  struct HostConnection {
    std::string host;
    boost::asio::ip::tcp::resolver resolver;
  };
  struct EndpointConnection {
    boost::asio::ip::tcp::endpoint endpoint;
  };

  State(Service& service,
        const std::string& host,
        Callback cb,
        int connectionTry)
    : service(service)
    , connectionCB(cb)
    , socket(service.ioContext())
    , connectionTry(connectionTry)
    , conn(
        HostConnection{ host,
                        boost::asio::ip::tcp::resolver(service.ioContext()) }) {
  }
  State(Service& service,
        boost::asio::ip::tcp::endpoint endpoint,
        Callback cb,
        int connectionTry)
    : service(service)
    , connectionCB(cb)
    , socket(service.ioContext())
    , connectionTry(connectionTry)
    , conn(EndpointConnection{ endpoint }) {}

  Service& service;
  Callback connectionCB;
  boost::asio::coroutine coro;
  boost::asio::ip::tcp::socket socket;
  int connectionTry;

  std::variant<HostConnection, EndpointConnection> conn;

  const std::string& host() {
    const auto& c = std::get<HostConnection>(conn);
    return c.host;
  }
  boost::asio::ip::tcp::resolver& resolver() {
    auto& c = std::get<HostConnection>(conn);
    return c.resolver;
  }
  boost::asio::ip::tcp::endpoint& endpoint() {
    auto& c = std::get<EndpointConnection>(conn);
    return c.endpoint;
  }
};

TCPConnectionInitiator::TCPConnectionInitiator(Service& service,
                                               const std::string& host,
                                               Callback cb,
                                               int connectionTry)
  : m_state(std::make_shared<State>(service, host, cb, connectionTry)) {
  try_connecting_to_host(boost::system::error_code(),
                         boost::asio::ip::tcp::resolver::iterator());
}
TCPConnectionInitiator::TCPConnectionInitiator(
  Service& service,
  boost::asio::ip::tcp::endpoint endpoint,
  Callback cb,
  int connectionTry)
  : m_state(std::make_shared<State>(service, endpoint, cb, connectionTry)) {
  try_connecting_to_endpoint(boost::system::error_code());
}
TCPConnectionInitiator::TCPConnectionInitiator(
  const TCPConnectionInitiator& initiator)
  : m_state(initiator.m_state) {}
TCPConnectionInitiator::~TCPConnectionInitiator() {}

#include <boost/asio/yield.hpp>
void
TCPConnectionInitiator::try_connecting_to_host(
  const ::boost::system::error_code& ec,
  ::boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {

  reenter(&m_state->coro) {
    // Resolve Host and get iterator to all resolved endpoints.
    parac_log(PARAC_COMMUNICATOR, PARAC_TRACE, "Resolving {}.", m_state->host());
    yield m_state->resolver().async_resolve(
      m_state->host(),
      boost::bind(&TCPConnectionInitiator::try_connecting_to_host,
                  *this,
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::iterator));

    if(ec) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALERROR,
                "Could not resolve {}! Error: {}",
                m_state->host(),
                ec);
      return;
    }

    // Iterate over endpoints while there are still endpoints left to try. Loop
    // does not exit on success, but function is not called anymore once
    // connection has been established.
    while(endpoint_iterator != boost::asio::ip::tcp::resolver::iterator()) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "Resolved {} to endpoint {}. Trying to connect.",
                m_state->host(),
                endpoint_iterator->endpoint());

      yield m_state->socket.async_connect(
        *endpoint_iterator,
        boost::bind(&TCPConnectionInitiator::try_connecting_to_host,
                    *this,
                    boost::asio::placeholders::error,
                    ++endpoint_iterator));

      if(ec) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_LOCALERROR,
                  "Resolved {} to endpoint {}. Connection error: {}",
                  m_state->host(),
                  m_state->socket.remote_endpoint(),
                  ec);
      } else {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_TRACE,
                  "Successfully connected socket to host {} (endpoint {}). "
                  "Starting Paracooba connection.",
                  m_state->host(),
                  m_state->socket.remote_endpoint());
      }

      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALERROR,
                "Tried all resolved endpoints for host {} without successfully "
                "establishing a connection.",
                m_state->host());
    }
  }
}
void
TCPConnectionInitiator::try_connecting_to_endpoint(
  const ::boost::system::error_code& ec) {
  reenter(m_state->coro) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Trying to connect to endpoint {}.",
              m_state->endpoint());

    yield m_state->socket.async_connect(
      m_state->endpoint(),
      boost::bind(&TCPConnectionInitiator::try_connecting_to_endpoint,
                  *this,
                  boost::asio::placeholders::error));

    if(ec) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALERROR,
                "Could not connect to endpoint {}! Connection error: {}",
                m_state->socket.remote_endpoint(),
                ec);
    } else {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "Successfully connected socket to endpoint {}. "
                "Starting Paracooba connection.",
                m_state->socket.remote_endpoint());
    }
  }
}
#include <boost/asio/unyield.hpp>
}
