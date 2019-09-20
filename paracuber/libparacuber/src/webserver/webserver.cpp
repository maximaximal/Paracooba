#include "../../include/paracuber/webserver/webserver.hpp"
#include "../../include/paracuber/config.hpp"

#ifndef ENABLE_INTERNAL_WEBSERVER
#error "Internal Webserver must be enabled if this source is compiled!"
#endif

namespace paracuber {
namespace webserver {
Webserver::Webserver(ConfigPtr config,
                     LogPtr log,
                     boost::asio::io_service& ioService)
  : m_config(config)
  , m_log(log)
  , m_logger(log->createLogger())
  , m_ioService(ioService)
{
  PARACUBER_LOG(m_logger, Trace)
    << "Creating internal webserver. Reachable over port "
    << std::to_string(config->getUint16(Config::HTTPListenPort));
}
Webserver::~Webserver() {}
}
}
