#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <limits>
#include <ratio>
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
static uint16_t
ExtractPortFromConnectionString(std::string& connectionString,
                                uint16_t defaultPort) {
  std::string port = std::to_string(defaultPort);

  auto posOfColon = connectionString.find(":");
  if(posOfColon != std::string::npos) {
    port = connectionString.substr(posOfColon + 1, std::string::npos);
    connectionString = connectionString.substr(0, posOfColon);
  }

  int portNum = std::atoi(port.c_str());

  if(portNum < 1 || portNum > std::numeric_limits<uint16_t>::max()) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Provided port {} for host {} is no valid port! Using default "
              "TCP target port {} instead.",
              port,
              connectionString,
              defaultPort);
    portNum = defaultPort;
  }

  return portNum;
}

struct TCPConnectionInitiator::State {
  struct HostConnection {
    std::string host;
    uint16_t port;
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::resolver::query query;
    boost::asio::ip::tcp::endpoint currentEndpoint;
  };
  struct EndpointConnection {
    boost::asio::ip::tcp::endpoint endpoint;
  };

  State(Service& service,
        const std::string& host,
        uint16_t port,
        Callback cb,
        int connectionTry)
    : service(service)
    , connectionCB(cb)
    , socket(service.ioContext())
    , timer(service.ioContext())
    , connectionTry(connectionTry)
    , conn(
        HostConnection{ host,
                        port,
                        boost::asio::ip::tcp::resolver(service.ioContext()),
                        boost::asio::ip::tcp::resolver::query(
                          host,
                          std::to_string(port),
                          boost::asio::ip::tcp::resolver::numeric_service) }) {}
  State(Service& service,
        boost::asio::ip::tcp::endpoint endpoint,
        Callback cb,
        int connectionTry)
    : service(service)
    , connectionCB(cb)
    , socket(service.ioContext())
    , timer(service.ioContext())
    , connectionTry(connectionTry)
    , conn(EndpointConnection{ endpoint }) {}

  Service& service;
  Callback connectionCB;
  boost::asio::coroutine coro;
  boost::asio::ip::tcp::socket socket;
  boost::asio::steady_timer timer;
  int connectionTry;
  bool retry = false;

  std::variant<HostConnection, EndpointConnection> conn;

  const std::string& host() {
    const auto& c = std::get<HostConnection>(conn);
    return c.host;
  }
  const uint16_t& port() {
    const auto& c = std::get<HostConnection>(conn);
    return c.port;
  }
  boost::asio::ip::tcp::resolver& resolver() {
    auto& c = std::get<HostConnection>(conn);
    return c.resolver;
  }
  boost::asio::ip::tcp::resolver::query& query() {
    auto& c = std::get<HostConnection>(conn);
    return c.query;
  }
  boost::asio::ip::tcp::endpoint& currentEndpoint() {
    auto& c = std::get<HostConnection>(conn);
    return c.currentEndpoint;
  }
  boost::asio::ip::tcp::endpoint& endpoint() {
    auto& c = std::get<EndpointConnection>(conn);
    return c.endpoint;
  }
};

TCPConnectionInitiator::TCPConnectionInitiator(Service& service,
                                               const std::string& host,
                                               Callback cb,
                                               int connectionTry) {
  std::string connectionString = host;
  uint16_t port = ExtractPortFromConnectionString(
    connectionString, service.defaultTCPTargetPort());

  m_state =
    std::make_shared<State>(service, connectionString, port, cb, connectionTry);
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
TCPConnectionInitiator::~TCPConnectionInitiator() {
  if(m_state.use_count() == 1 && m_state->retry &&
     m_state->connectionTry < m_state->service.connectionRetries()) {

    m_state->retry = false;
    auto timeout = m_state->service.retryTimeoutMS();

    parac_log(
      PARAC_COMMUNICATOR,
      PARAC_TRACE,
      "As TCPConnectionInitiator is closing, a new connection is tried as a "
      "retry, because this connection was not successful. Using timeout {}ms",
      timeout);

    m_state->timer.expires_from_now(
      std::chrono::milliseconds(timeout));
    m_state->timer.async_wait(
      std::bind(&TCPConnectionInitiator::retryConnection, *this));
  }
}

void
TCPConnectionInitiator::retryConnection() {
  ++m_state->connectionTry;

  if(std::holds_alternative<State::HostConnection>(m_state->conn)) {
    std::string connectionString =
      m_state->host() + ":" + std::to_string(m_state->port());
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Retry (number {}) connection to {}.",
              m_state->connectionTry,
              connectionString);
    TCPConnectionInitiator(m_state->service,
                           connectionString,
                           m_state->connectionCB,
                           m_state->connectionTry);
  } else {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Retry (number {}) connection to {}.",
              m_state->connectionTry,
              m_state->endpoint());
    TCPConnectionInitiator(m_state->service,
                           m_state->endpoint(),
                           m_state->connectionCB,
                           m_state->connectionTry);
  }
}

#include <boost/asio/yield.hpp>
void
TCPConnectionInitiator::try_connecting_to_host(
  const ::boost::system::error_code& ec,
  ::boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {

  reenter(&m_state->coro) {
    // Resolve Host and get iterator to all resolved endpoints.
    parac_log(
      PARAC_COMMUNICATOR, PARAC_TRACE, "Resolving {}.", m_state->host());

    yield m_state->resolver().async_resolve(
      m_state->query(),
      boost::bind(&TCPConnectionInitiator::try_connecting_to_host,
                  *this,
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::iterator));

    if(ec) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALERROR,
                "Could not resolve {}! Error: {}",
                m_state->host(),
                ec.message());
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

      m_state->currentEndpoint() = endpoint_iterator->endpoint();

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
                  m_state->currentEndpoint(),
                  ec.message());
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
      m_state->retry = true;
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
