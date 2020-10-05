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
  LISTEN_ADDRESS,
  BROADCAST_ADDRESS,
  UDP_LISTEN_PORT,
  UDP_TARGET_PORT,
  TCP_LISTEN_PORT,
  TCP_TARGET_PORT,
  NETWORK_TIMEOUT,
  CONNECTION_RETRIES,
  KNOWN_REMOTES,
  _COUNT
};

class Service {
  public:
  Service(parac_handle& handle);
  ~Service();

  void applyConfig(parac_config_entry* e);

  parac_status start();

  boost::asio::io_context& ioContext();

  private:
  parac_status run();

  struct Internal;
  const std::unique_ptr<Internal> m_internal;
  const parac_handle& m_handle;
  parac_config_entry* m_config = nullptr;
};
}
