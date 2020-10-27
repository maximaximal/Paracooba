#include "tcp_acceptor.hpp"
#include "communicator_util.hpp"
#include "paracooba/common/status.h"
#include "service.hpp"
#include "tcp_connection.hpp"

#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>

#include <paracooba/common/log.h>
#include <paracooba/communicator/communicator.h>
#include <paracooba/module.h>

namespace parac::communicator {
struct TCPAcceptor::Internal {
  Internal(Service& service,
           const std::string listenAddress,
           uint16_t listenPort)
    : service(service)
    , listenAddress(listenAddress)
    , listenPort(listenPort) {}

  boost::asio::ip::tcp::endpoint endpoint;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
  std::unique_ptr<boost::asio::ip::tcp::socket> newSocket;

  Service& service;
  boost::asio::coroutine coro;

  std::string listenAddress;
  uint16_t listenPort;
};

TCPAcceptor::TCPAcceptor() {}

TCPAcceptor::~TCPAcceptor() {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Destroy TCPAcceptor.");
}

parac_status
TCPAcceptor::start(Service& service,
                   const std::string& listenAddressStr,
                   uint16_t listenPort) {
  m_internal =
    std::make_unique<Internal>(service, listenAddressStr, listenPort);

  auto ipAddress = ParseIPAddress(listenAddressStr);
  if(!ipAddress) {
    return PARAC_INVALID_IP;
  }

  m_internal->endpoint = boost::asio::ip::tcp::endpoint(*ipAddress, listenPort);

  m_internal->acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(
    service.ioContext(), m_internal->endpoint.protocol());

  auto comm = service.handle().modules[PARAC_MOD_COMMUNICATOR]->communicator;

  if(service.automaticListenPortAssignment()) {
    bool portFound = false;

    for(auto port = m_internal->endpoint.port();
        port < std::numeric_limits<decltype(port)>::max();
        ++port) {
      m_internal->endpoint.port(port);

      comm->tcp_listen_port = port;

      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "Automatic TCP port assignment is enabled. Now trying endpoint "
                "{}. Tried port {}, maximum port number is {}.",
                m_internal->endpoint,
                port,
                std::numeric_limits<decltype(port)>::max());

      boost::system::error_code ec;
      m_internal->acceptor->bind(m_internal->endpoint, ec);
      if(!ec) {
        portFound = true;
        break;
      }

      if(ec == boost::asio::error::address_in_use) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_TRACE,
                  "Automatic TCP port assignment is enabled and port {} was "
                  "already in use. Trying next port."
                  "{}.",
                  port);
      } else {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_FATAL,
                  "Cannot bind to endpoint {}! Error: {}",
                  m_internal->endpoint,
                  ec.message());
        return PARAC_GENERIC_ERROR;
      }
    }

    if(!portFound) {
      parac_log(PARAC_COMMUNICATOR, PARAC_FATAL, "No free TCP port found!");
      return PARAC_GENERIC_ERROR;
    }
  } else {
    m_internal->acceptor->bind(m_internal->endpoint);
  }

  // Successfully bound!
  m_internal->acceptor->listen();

  parac_log(PARAC_COMMUNICATOR,
            PARAC_DEBUG,
            "Starting TCPAcceptor on endpoint {}.",
            m_internal->endpoint);

  comm->tcp_acceptor_active = true;

  loop(boost::system::error_code());

  return PARAC_OK;
}

#include <boost/asio/yield.hpp>
void
TCPAcceptor::loop(const boost::system::error_code& ec) {
  const auto l = [this](const boost::system::error_code& ec) { loop(ec); };

  if(!ec) {
    reenter(m_internal->coro) {
      for(;;) {
        m_internal->newSocket = std::make_unique<boost::asio::ip::tcp::socket>(
          m_internal->service.ioContext());

        yield m_internal->acceptor->async_accept(*m_internal->newSocket, l);

        if(ec) {
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_LOCALERROR,
                    "Error during accepting new TCP connection on endpoint {}! "
                    "Error: {}",
                    m_internal->endpoint,
                    ec.message());
        } else {
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_DEBUG,
                    "New TCP connection on endpoint {} from {}.",
                    m_internal->endpoint,
                    m_internal->newSocket->remote_endpoint());

          m_internal->newSocket->non_blocking(true);

          TCPConnection(m_internal->service, std::move(m_internal->newSocket));
        }
      }
    }
  }
}
#include <boost/asio/unyield.hpp>

}
