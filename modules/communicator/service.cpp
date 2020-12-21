#include <atomic>
#include <boost/filesystem/operations.hpp>
#include <map>

#include "paracooba/common/timeout.h"
#include "service.hpp"
#include "tcp_acceptor.hpp"
#include "tcp_connection_initiator.hpp"
#include "timeout_controller.hpp"
#include "udp_acceptor.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <paracooba/common/config.h>
#include <paracooba/common/log.h>
#include <paracooba/common/thread_registry.h>
#include <paracooba/communicator/communicator.h>
#include <paracooba/module.h>

using boost::asio::io_context;

namespace parac::communicator {
struct Service::Internal {
  io_context context;
  parac_thread_registry_handle threadHandle;

  std::unique_ptr<TCPAcceptor> tcpAcceptor;
  std::unique_ptr<UDPAcceptor> udpAcceptor;
  std::unique_ptr<TimeoutController> timeoutController;

  std::map<parac_id,
           std::pair<boost::asio::steady_timer, TCPConnectionPayloadPtr>>
    connectionPayloads;
};

Service::Service(parac_handle& handle)
  : m_internal(std::make_unique<Internal>())
  , m_handle(handle) {
  m_internal->threadHandle.userdata = this;

  m_internal->timeoutController = std::make_unique<TimeoutController>(*this);
}

Service::~Service() {}

void
Service::applyConfig(parac_config_entry* e) {
  m_config = e;

  assert(m_internal);
  assert(e);

  if(!boost::filesystem::exists(temporaryDirectory())) {
    boost::filesystem::create_directory(temporaryDirectory());
  }

  m_internal->tcpAcceptor = std::make_unique<TCPAcceptor>();

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
    m_internal->tcpAcceptor->start(*this,
                                   m_config[LISTEN_ADDRESS].value.string,
                                   m_config[TCP_LISTEN_PORT].value.uint16);
  if(m_internal->udpAcceptor)
    m_internal->udpAcceptor->start(*this);

  connectToKnownRemotes();

  m_internal->context.run();
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
  TCPConnectionInitiator initiator(*this, remote);
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

  decltype(Internal::connectionPayloads)::mapped_type entry{
    boost::asio::steady_timer(ioContext()), std::move(payload)
  };

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
  m_internal->connectionPayloads.insert({ id, std::move(entry) });
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

bool
Service::automaticListenPortAssignment() const {
  assert(m_config);
  // The option is to disable (as the default is to enable it) and this function
  // asks if it is enabled, so the ! is required.
  return !m_config[AUTOMATIC_LISTEN_PORT_ASSIGNMENT].value.boolean_switch;
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
}
