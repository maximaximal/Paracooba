#ifndef PARACUBER_WEBSERVER_WEBSERVER_HPP
#define PARACUBER_WEBSERVER_WEBSERVER_HPP

#ifndef ENABLE_INTERNAL_WEBSERVER
#error "Internal Webserver must be enabled if this header is included!"
#endif

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
class Webserver
{
  public:
  class HTTPListener;
  class HTTPSession;

  Webserver(ConfigPtr config, LogPtr log, boost::asio::io_service& ioService);
  ~Webserver();

  std::string buildLink();

  private:
  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  boost::asio::io_service& m_ioService;

  std::shared_ptr<HTTPListener> m_httpListener;
};
}
}

#endif
