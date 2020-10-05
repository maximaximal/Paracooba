#include "service.hpp"
#include "tcp_acceptor.hpp"
#include "udp_acceptor.hpp"

#include <boost/asio/io_context.hpp>

#include <paracooba/common/config.h>
#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>
#include <paracooba/module.h>

using boost::asio::io_context;

namespace parac::communicator {
struct Service::Internal {
  io_context context;
  parac_thread_registry_handle threadHandle;

  std::unique_ptr<TCPAcceptor> tcpAcceptor;
  std::unique_ptr<UDPAcceptor> udpAcceptor;
};

Service::Service(struct parac_handle& handle)
  : m_internal(std::make_unique<Internal>())
  , m_handle(handle) {
  m_internal->threadHandle.userdata = this;
}

Service::~Service() {}

void
Service::applyConfig(parac_config_entry* e) {
  m_config = e;

  m_internal->tcpAcceptor =
    std::make_unique<TCPAcceptor>(m_config[LISTEN_ADDRESS].value.string,
                                  m_config[TCP_LISTEN_PORT].value.uint16);

  m_internal->udpAcceptor =
    std::make_unique<UDPAcceptor>(m_config[LISTEN_ADDRESS].value.string,
                                  m_config[UDP_LISTEN_PORT].value.uint16);
}

parac_status
Service::start() {
  return parac_thread_registry_create(
    m_handle.thread_registry,
    m_handle.modules[PARAC_MOD_COMMUNICATOR],
    [](parac_thread_registry_handle* handle) -> int {
      Service* service = static_cast<Service*>(handle->userdata);
      return service->run();
    },
    &m_internal->threadHandle);
}

void
Service::stop() {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Stopping communicator service.");

  ioContext().stop();
}

parac_status
Service::run() {
  parac_log(
    PARAC_COMMUNICATOR, PARAC_DEBUG, "Starting communicator io_context.");

  if(m_internal->tcpAcceptor)
    m_internal->tcpAcceptor->start(*this);
  if(m_internal->udpAcceptor)
    m_internal->udpAcceptor->start(*this);

  m_internal->context.run();
  return PARAC_OK;
}

io_context&
Service::ioContext() {
  return m_internal->context;
}
}
