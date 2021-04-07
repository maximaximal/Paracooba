#pragma once

#include "paracooba/common/status.h"
#include "paracooba/common/timeout.h"
#include "paracooba/common/types.h"
#include <boost/version.hpp>
#include <memory>
#include <optional>
#include <thread>

namespace boost::asio {
#if BOOST_VERSION >= 106600
class io_context;
#else
class io_service;
typedef io_service io_context;
#endif
namespace ip {
class address;
}
}

struct parac_handle;
struct parac_config_entry;
struct parac_timeout;

namespace parac::communicator {
class MessageSendQueue;

enum Config {
  TEMPORARY_DIRECTORY,
  TCP_LISTEN_ADDRESS,
  UDP_LISTEN_ADDRESS,
  BROADCAST_ADDRESS,
  UDP_LISTEN_PORT,
  UDP_TARGET_PORT,
  TCP_LISTEN_PORT,
  TCP_TARGET_PORT,
  NETWORK_TIMEOUT,
  RETRY_TIMEOUT,
  MESSAGE_TIMEOUT,
  CONNECTION_TIMEOUT,
  KEEPALIVE_INTERVAL,
  CONNECTION_RETRIES,
  KNOWN_REMOTES,
  AUTOMATIC_LISTEN_PORT_ASSIGNMENT,
  ENABLE_UDP,
  UDP_ANNOUNCEMENT_INTERVAL_MS,
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
  bool stopped() const;

  const std::thread::id& ioContextThreadId() const;

  int connectionRetries() const;
  const char* temporaryDirectory() const;
  uint16_t defaultTCPTargetPort() const;
  uint16_t defaultTCPListenPort() const;
  uint16_t currentTCPListenPort() const;
  uint16_t currentUDPListenPort() const;
  uint16_t udpTargetPort() const;
  uint32_t networkTimeoutMS() const;
  uint32_t retryTimeoutMS() const;
  uint16_t messageTimeoutMS() const;
  uint32_t connectionTimeoutMS() const;
  uint32_t keepaliveIntervalMS() const;
  bool automaticListenPortAssignment() const;

  bool enableUDP() const;
  bool enableUDPAnnouncements() const;
  uint32_t udpAnnouncementInterval() const;

  const char* knownRemote(size_t i) const;
  size_t knownRemoteCount() const;

  parac_handle& handle() { return m_handle; }

  void connectToRemote(const std::string& remote);
  parac_timeout* setTimeout(uint64_t ms,
                            void* userdata,
                            parac_timeout_expired expiery_cb);

  std::shared_ptr<MessageSendQueue> getMessageSendQueueForRemoteId(parac_id id);

  void setTCPAcceptorActive();
  bool isTCPAcceptorActive();

  void addOutgoingMessageToCounter(size_t count = 1);
  void removeOutgoingMessageFromCounter(size_t count = 1);

  parac_id id() const;
  const char* broadcastAddress() const;

  boost::asio::ip::address tcpAcceptorAddress() const;

  private:
  parac_status run();
  void stop();

  void connectToKnownRemotes();
  void startMessageSendQueueTimer();
  void handleMessageSendTimerTick();

  struct Internal;
  std::shared_ptr<Internal> m_internal;
  parac_handle& m_handle;
  parac_config_entry* m_config = nullptr;
};
}
