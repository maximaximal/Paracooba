#include "../../include/paracuber/webserver/initiator.hpp"
#include "../../include/paracuber/config.hpp"

#ifdef ENABLE_INTERNAL_WEBSERVER
#include "../../include/paracuber/webserver/webserver.hpp"
#endif

namespace paracuber {
namespace webserver {
Initiator::Initiator(ConfigPtr config,
                     LogPtr log,
                     boost::asio::io_service& ioService)
  : m_logger(log->createLogger())
{
  if(!config->isInternalWebserverEnabled())
    return;

#ifdef ENABLE_INTERNAL_WEBSERVER
  m_webserver = std::make_unique<Webserver>(config, log, ioService);
#else
  PARACUBER_LOG(m_logger, Info) << "Webserver disabled during compilation, "
                                   "cannot start internal webserver.";
#endif
}
Initiator::~Initiator() {}
}
}
