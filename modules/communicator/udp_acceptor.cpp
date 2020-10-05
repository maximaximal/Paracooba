#include "udp_acceptor.hpp"

#include <paracooba/common/log.h>

namespace parac::communicator {
struct UDPAcceptor::Internal {};

UDPAcceptor::UDPAcceptor(const std::string& listenAddressStr,
                         uint16_t listenPort)
  : m_internal(std::make_unique<Internal>())
  , m_listenAddressStr(listenAddressStr)
  , m_listenPort(listenPort) {}
UDPAcceptor::~UDPAcceptor() {}

parac_status
UDPAcceptor::start(Service& service) {
  parac_log(PARAC_COMMUNICATOR,
            PARAC_DEBUG,
            "Starting UDPAcceptor with listen address {} and port {}.",
            m_listenAddressStr,
            m_listenPort);

  return PARAC_OK;
}

}
