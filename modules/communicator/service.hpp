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

namespace parac::communicator {
class Service {
  public:
  Service(parac_handle& handle);
  ~Service();

  parac_status start();

  boost::asio::io_context& ioContext();

  private:
  parac_status run();

  struct Internal;
  const std::unique_ptr<Internal> m_internal;
  const parac_handle& m_handle;
};
}
