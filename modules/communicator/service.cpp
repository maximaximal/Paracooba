#include <atomic>
#include <boost/exception/exception.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/error_code.hpp>
#include <map>

#include "paracooba/common/timeout.h"
#include "service.hpp"
#include "tcp_acceptor.hpp"
#include "tcp_connection_initiator.hpp"
#include "timeout_controller.hpp"
#include "udp_acceptor.hpp"

#if BOOST_VERSION >= 106600
#include <boost/asio/io_context.hpp>
#else
#include <boost/asio/io_service.hpp>
#endif
#include <boost/asio/steady_timer.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>

#include <paracooba/common/config.h>
#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>
#include <paracooba/communicator/communicator.h>
#include <paracooba/module.h>

using boost::asio::io_context;

namespace parac::communicator {
struct Service::Internal {
  using TCPConnectionPayloadPair =
    std::pair<boost::asio::steady_timer, TCPConnectionPayloadPtr>;

  io_context context;
  parac_thread_registry_handle threadHandle;

  std::unique_ptr<TCPAcceptor> tcpAcceptor;
  std::unique_ptr<UDPAcceptor> udpAcceptor;
  std::unique_ptr<TimeoutController> timeoutController;

  std::map<parac_id, TCPConnectionPayloadPair> connectionPayloads;

  std::atomic_bool tcpAcceptorActive, stopRequested = false;
  std::atomic_size_t outgoingMessageCounter = 0;
};

Service::Service(parac_handle& handle)
  : m_internal(std::make_unique<Internal>())
  , m_handle(handle) {
  m_internal->threadHandle.userdata = this;

  m_internal->timeoutController = std::make_unique<TimeoutController>(*this);
}

Service::~Service() {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Destroy Service.");
}

void
Service::applyConfig(parac_config_entry* e) {
  m_config = e;

  assert(m_internal);
  assert(e);

  if(!boost::filesystem::exists(temporaryDirectory())) {
    boost::filesystem::create_directory(temporaryDirectory());
  }

  m_internal->tcpAcceptor = std::make_unique<TCPAcceptor>();

  if(enableUDP()) {
    m_internal->udpAcceptor =
      std::make_unique<UDPAcceptor>(m_config[UDP_LISTEN_ADDRESS].value.string,
                                    m_config[UDP_LISTEN_PORT].value.uint16);
  }
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
Service::requestStop() {
  parac_log(
    PARAC_COMMUNICATOR,
    PARAC_DEBUG,
    "Stopping communicator service requested. Outgoing message counter at {}",
    m_internal->outgoingMessageCounter);

  m_internal->stopRequested = true;

  if(m_internal->outgoingMessageCounter == 0) {
    stop();
  }
}

void
Service::stop() {
  if(ioContext().stopped()) {
    return;
  }

  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Stopping communicator service.");

  ioContext().stop();
}

parac_status
Service::run() {
  parac_log(
    PARAC_COMMUNICATOR, PARAC_DEBUG, "Starting communicator io_context.");

  if(m_internal->tcpAcceptor)
    m_internal->tcpAcceptor->start(
      *this, m_config[TCP_LISTEN_ADDRESS].value.string, defaultTCPListenPort());

  if(m_internal->udpAcceptor) {
    m_internal->udpAcceptor->start(*this);

    if(enableUDPAnnouncements()) {
      m_internal->udpAcceptor->startAnnouncements(
        m_internal->tcpAcceptor->connectionString(), udpAnnouncementInterval());
    }
  }

  connectToKnownRemotes();

  while(!m_internal->context.stopped()) {
    try {
      m_internal->context.run();
    } catch(boost::exception_detail::clone_impl<
            boost::exception_detail::error_info_injector<
              boost::system::system_error>>& e) {
      // Exceptions in connections should be able to be ignored, as connections
      // are just dropped and re-initiated by their destructors if so required.
    } catch(std::exception& e) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALERROR,
                "Exception in io context: {}, diagnostic info: {}",
                e.what(),
                boost::diagnostic_information(e));
    }
  }
  return PARAC_OK;
}

void
Service::connectToKnownRemotes() {
  for(size_t i = 0; i < knownRemoteCount(); ++i) {
    const char* remote = knownRemote(i);
    connectToRemote(remote);
  }
}

void
Service::connectToRemote(const std::string& remote) {
  try {
    TCPConnectionInitiator initiator(*this, remote);
  } catch(std::exception& e) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Exception while trying to connect to remote {}: {}",
              remote,
              e.what());
  }
}

parac_timeout*
Service::setTimeout(uint64_t ms,
                    void* userdata,
                    parac_timeout_expired expiery_cb) {
  return m_internal->timeoutController->setTimeout(ms, userdata, expiery_cb);
}

void
Service::registerTCPConnectionPayload(parac_id id,
                                      TCPConnectionPayloadPtr payload) {
  assert(id != 0);

  auto [it, inserted] = m_internal->connectionPayloads.try_emplace(
    id, ioContext(), std::move(payload));
  assert(inserted);

  auto& entry = it->second;

  entry.first.expires_from_now(std::chrono::milliseconds(retryTimeoutMS() * 4));
  entry.first.async_wait([this, id](const boost::system::error_code& error) {
    if(!error) {
      auto ptr = retrieveTCPConnectionPayload(id);
      if(ptr) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_TRACE,
                  "TCPConnectionPayload for connection to {} expired and was "
                  "invalidated.",
                  id);
      }
    }
  });
}

TCPConnectionPayloadPtr
Service::retrieveTCPConnectionPayload(parac_id id) {
  auto it = m_internal->connectionPayloads.find(id);
  TCPConnectionPayloadPtr ptr(nullptr, nullptr);
  if(it->second.second) {
    ptr = std::move(it->second.second);
    m_internal->connectionPayloads.erase(id);
  }
  return ptr;
}

void
Service::addOutgoingMessageToCounter(size_t count) {
  m_internal->outgoingMessageCounter += count;
}
void
Service::removeOutgoingMessageFromCounter(size_t count) {
  assert(m_internal->outgoingMessageCounter >= count);

  m_internal->outgoingMessageCounter -= count;

  if(m_internal->stopRequested && m_internal->outgoingMessageCounter == 0) {
    stop();
  }
}

parac_id
Service::id() const {
  return m_handle.id;
}

void
Service::setTCPAcceptorActive() {
  m_internal->tcpAcceptorActive = true;
}
bool
Service::isTCPAcceptorActive() {
  return m_internal->tcpAcceptorActive;
}

io_context&
Service::ioContext() {
  return m_internal->context;
}

int
Service::connectionRetries() const {
  assert(m_config);
  return m_config[CONNECTION_RETRIES].value.uint32;
}
const char*
Service::temporaryDirectory() const {
  assert(m_config);
  return m_config[TEMPORARY_DIRECTORY].value.string;
}

uint16_t
Service::defaultTCPTargetPort() const {
  assert(m_config);
  return m_config[TCP_TARGET_PORT].value.uint16;
}

uint16_t
Service::defaultTCPListenPort() const {
  assert(m_config);
  return m_config[TCP_LISTEN_PORT].value.uint16;
}

uint16_t
Service::currentTCPListenPort() const {
  return m_handle.modules[PARAC_MOD_COMMUNICATOR]
    ->communicator->tcp_listen_port;
}

uint16_t
Service::currentUDPListenPort() const {
  return m_handle.modules[PARAC_MOD_COMMUNICATOR]
    ->communicator->udp_listen_port;
}

uint32_t
Service::networkTimeoutMS() const {
  assert(m_config);
  return m_config[NETWORK_TIMEOUT].value.uint32;
}

uint32_t
Service::retryTimeoutMS() const {
  assert(m_config);
  return m_config[RETRY_TIMEOUT].value.uint32;
}

uint32_t
Service::keepaliveIntervalMS() const {
  assert(m_config);
  return m_config[KEEPALIVE_INTERVAL].value.uint32;
}

bool
Service::automaticListenPortAssignment() const {
  assert(m_config);
  // The option is to disable (as the default is to enable it) and this function
  // asks if it is enabled, so the ! is required.
  return !m_config[AUTOMATIC_LISTEN_PORT_ASSIGNMENT].value.boolean_switch;
}

bool
Service::enableUDP() const {
  assert(m_config);
  return m_config[ENABLE_UDP].value.boolean_switch || enableUDPAnnouncements();
}

const char*
Service::broadcastAddress() const {
  assert(m_config);
  return m_config[BROADCAST_ADDRESS].value.string;
}

bool
Service::enableUDPAnnouncements() const {
  return udpAnnouncementInterval() > 0;
}

uint32_t
Service::udpAnnouncementInterval() const {
  assert(m_config);
  return m_config[UDP_ANNOUNCEMENT_INTERVAL_MS].value.uint32;
}

uint16_t
Service::udpTargetPort() const {
  assert(m_config);
  return m_config[UDP_TARGET_PORT].value.uint16;
}

const char*
Service::knownRemote(size_t i) const {
  assert(m_config);
  assert(m_config[KNOWN_REMOTES].value.string_vector.size);
  return m_config[KNOWN_REMOTES].value.string_vector.strings[i];
}
size_t
Service::knownRemoteCount() const {
  assert(m_config);
  return m_config[KNOWN_REMOTES].value.string_vector.size;
}

boost::asio::ip::address
Service::tcpAcceptorAddress() const {
  assert(m_internal->tcpAcceptor);
  return m_internal->tcpAcceptor->address();
}
}
