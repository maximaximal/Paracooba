#ifndef PARACOOBA_WEBSERVER_INITIATOR_HPP
#define PARACOOBA_WEBSERVER_INITIATOR_HPP

#include "../log.hpp"
#include <boost/asio/io_service.hpp>
#include <memory>

namespace paracooba {
namespace webserver {
class Webserver;
class API;

class Initiator
{
  public:
  Initiator(ConfigPtr config, LogPtr log, boost::asio::io_service& ioService);
  ~Initiator();

  void run();
  void stop();

  API* getAPI();

  private:
  LogPtr m_log;
  ConfigPtr m_config;
  Logger m_logger;
  boost::asio::io_service& m_ioService;

#ifdef ENABLE_INTERNAL_WEBSERVER
  std::unique_ptr<Webserver> m_webserver;
#endif
};
}
}

#endif
