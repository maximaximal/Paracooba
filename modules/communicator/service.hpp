#pragma once

#include "paracooba/common/status.h"
#include "paracooba/common/timeout.h"
#include "paracooba/common/types.h"
#include <boost/version.hpp>
#include <memory>

namespace boost::asio {
#if BOOST_VERSION >= 106600
class io_context;
#else
class io_service;
typedef io_service io_context;
#endif
}

struct parac_handle;
struct parac_config_entry;
struct parac_timeout;

namespace parac::communicator {
struct TCPConnectionPayload;
using TCPConnectionPayloadPtr =
  std::unique_ptr<TCPConnectionPayload, void (*)(TCPConnectionPayload*)>;

enum Config {
  TEMPORARY_DIRECTORY,
  LISTEN_ADDRESS,
  BROADCAST_ADDRESS,
  UDP_LISTEN_PORT,
  UDP_TARGET_PORT,
  TCP_LISTEN_PORT,
  TCP_TARGET_PORT,
  NETWORK_TIMEOUT,
  RETRY_TIMEOUT,
  KEEPALIVE_INTERVAL,
  CONNECTION_RETRIES,
  KNOWN_REMOTES,
  AUTOMATIC_LISTEN_PORT_ASSIGNMENT,
  _COUNT
};

class Service {
  public:
  Service(parac_handle& handle);
  ~Service();

  void applyConfig(parac_config_entry* e);

  parac_status start();
  void requestStop();

  boost::asio::io_context& ioContext();

  int connectionRetries() const;
  const char* temporaryDirectory() const;
  uint16_t defaultTCPTargetPort() const;
  uint16_t defaultTCPListenPort() const;
  uint16_t currentTCPListenPort() const;
  uint16_t currentUDPListenPort() const;
  uint32_t networkTimeoutMS() const;
  uint32_t retryTimeoutMS() const;
  uint32_t keepaliveIntervalMS() const;
  bool automaticListenPortAssignment() const;

  const char* knownRemote(size_t i) const;
  size_t knownRemoteCount() const;

  parac_handle& handle() { return m_handle; }

  void connectToRemote(const std::string& remote);
  parac_timeout* setTimeout(uint64_t ms,
                            void* userdata,
                            parac_timeout_expired expiery_cb);

  void registerTCPConnectionPayload(parac_id id,
                                    TCPConnectionPayloadPtr payload);
  TCPConnectionPayloadPtr retrieveTCPConnectionPayload(parac_id id);

  void setTCPAcceptorActive();
  bool isTCPAcceptorActive();

  void addOutgoingMessageToCounter(size_t count = 1);
  void removeOutgoingMessageFromCounter(size_t count = 1);

  private:
  parac_status run();
  void stop();

  void connectToKnownRemotes();

  struct Internal;
  const std::unique_ptr<Internal> m_internal;
  parac_handle& m_handle;
  parac_config_entry* m_config = nullptr;
};
}
