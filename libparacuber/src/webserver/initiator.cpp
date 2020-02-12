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
  : m_config(config)
  , m_log(log)
  , m_ioService(ioService)
  , m_logger(log->createLogger("Initiator"))
{}
Initiator::~Initiator() {}

void
Initiator::run()
{
  if(!m_config->isInternalWebserverEnabled())
    return;

#ifdef ENABLE_INTERNAL_WEBSERVER
  if(!m_webserver) {
    m_webserver = std::make_unique<Webserver>(m_config, m_log, m_ioService);
  }

  m_webserver->run();
#else
  PARACUBER_LOG(m_logger, LocalError)
    << "Webserver disabled during compilation, "
       "cannot start internal webserver.";
#endif
}
void
Initiator::stop()
{
#ifdef ENABLE_INTERNAL_WEBSERVER
  if(m_webserver) {
    m_webserver->stop();
    m_webserver.reset();
  }
#endif
}
API*
Initiator::getAPI()
{
#ifdef ENABLE_INTERNAL_WEBSERVER
  if(m_webserver) {
    return m_webserver->getAPI();
  }
#endif
  return nullptr;
}
}
}
