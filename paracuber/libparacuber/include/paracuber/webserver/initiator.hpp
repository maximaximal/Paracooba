#ifndef PARACUBER_WEBSERVER_INITIATOR_HPP
#define PARACUBER_WEBSERVER_INITIATOR_HPP

#include "../log.hpp"
#include <boost/version.hpp>
#include <memory>

namespace boost {
namespace asio {
#if(BOOST_VERSION / 100 % 1000) >= 69
class io_context;
using io_service = io_context;
class signal_set;
#else
class io_service;
#endif
}
namespace system {
class error_code;
}
}

namespace paracuber {
namespace webserver {
class Webserver;

class Initiator
{
  public:
  Initiator(ConfigPtr config, LogPtr log, boost::asio::io_service& ioService);
  ~Initiator();

  private:
  Logger m_logger;

#ifdef ENABLE_INTERNAL_WEBSERVER
  std::unique_ptr<Webserver> m_webserver;
#endif
};
}
}

#endif
