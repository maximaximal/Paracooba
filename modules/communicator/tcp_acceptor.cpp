#include "tcp_acceptor.hpp"
#include "communicator_util.hpp"
#include "service.hpp"

#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <paracooba/common/log.h>

namespace parac::communicator {
struct TCPAcceptor::Internal {
  boost::asio::ip::tcp::endpoint endpoint;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
  std::unique_ptr<boost::asio::ip::tcp::socket> newSocket;

  Service* service = nullptr;
  boost::asio::coroutine coro;
};

TCPAcceptor::TCPAcceptor(const std::string& listenAddressStr,
                         uint16_t listenPort)
  : m_internal(std::make_unique<Internal>())
  , m_listenAddressStr(listenAddressStr)
  , m_listenPort(listenPort) {}

TCPAcceptor::~TCPAcceptor() {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Destroy TCPAcceptor.");
}

parac_status
TCPAcceptor::start(Service& service) {
  parac_log(PARAC_COMMUNICATOR,
            PARAC_DEBUG,
            "Starting TCPAcceptor with listen address {} and port {}.",
            m_listenAddressStr,
            m_listenPort);

  m_internal->service = &service;

  auto ipAddress = ParseIPAddress(m_listenAddressStr);
  if(!ipAddress) {
    return PARAC_INVALID_IP;
  }
  m_internal->endpoint =
    boost::asio::ip::tcp::endpoint(*ipAddress, m_listenPort);

  m_internal->acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(
    service.ioContext(), m_internal->endpoint.protocol());

  m_internal->acceptor->bind(m_internal->endpoint);
  m_internal->acceptor->listen();

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
          m_internal->service->ioContext());

        yield m_internal->acceptor->async_accept(*m_internal->newSocket, l);

        parac_log(PARAC_COMMUNICATOR,
                  PARAC_DEBUG,
                  "New TCP connection on endpoint {} from {}.",
                  m_internal->endpoint,
                  m_internal->newSocket->remote_endpoint());

        m_internal->newSocket->non_blocking(true);
      }
    }
  }
}
#include <boost/asio/unyield.hpp>

}
