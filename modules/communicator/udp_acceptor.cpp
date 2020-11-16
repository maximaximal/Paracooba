#include "udp_acceptor.hpp"
#include "service.hpp"

#include <memory>
#include <paracooba/common/log.h>

namespace parac::communicator {
struct UDPAcceptor::Internal {
  Internal(Service& service)
    : service(service) {}
  Service& service;
};

UDPAcceptor::UDPAcceptor(const std::string& listenAddressStr,
                         uint16_t listenPort)
  : m_listenAddressStr(listenAddressStr)
  , m_listenPort(listenPort) {}

UDPAcceptor::~UDPAcceptor() {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Destroy UDPAcceptor.");
}

parac_status
UDPAcceptor::start(Service& service) {
  m_internal = std::make_unique<Internal>(service);
  parac_log(PARAC_COMMUNICATOR,
            PARAC_DEBUG,
            "Starting UDPAcceptor with listen address {} and port {}.",
            m_listenAddressStr,
            m_listenPort);

  return PARAC_OK;
}

}
