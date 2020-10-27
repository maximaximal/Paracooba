#pragma once

#include "paracooba/common/status.h"
#include <boost/version.hpp>
#include <memory>

namespace boost::asio {
#if BOOST_VERSION > 106500
class io_context;
#else
class io_service;
typedef io_service io_context;
#endif
}

struct parac_handle;
struct parac_config_entry;

namespace parac::communicator {
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

  void stop();

  boost::asio::io_context& ioContext();

  int connectionRetries() const;
  const char* temporaryDirectory() const;
  uint16_t defaultTCPTargetPort() const;
  uint32_t networkTimeoutMS() const;
  uint32_t retryTimeoutMS() const;
  bool automaticListenPortAssignment() const;

  const char* knownRemote(size_t i) const;
  size_t knownRemoteCount() const;

  parac_handle& handle() { return m_handle; }

  void connectToRemote(const std::string &remote);

  private:
  parac_status run();

  void connectToKnownRemotes();

  struct Internal;
  const std::unique_ptr<Internal> m_internal;
  parac_handle& m_handle;
  parac_config_entry* m_config = nullptr;
};
}
