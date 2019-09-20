#ifndef PARACUBER_WEBSERVER_API_HPP
#define PARACUBER_WEBSERVER_API_HPP

#include "../log.hpp"
#include <boost/version.hpp>
#include <memory>
#include <regex>

namespace paracuber {
namespace webserver {
class Webserver;

class API
{
  public:
  enum Request
  {
    LocalConfig,
    LocalInfo,

    Unknown,
  };

  API(Webserver* webserver, ConfigPtr config, LogPtr log);
  ~API();

  static bool isAPIRequest(const std::string& target);
  static Request matchTargetToRequest(const std::string& target);

  std::string generateResponse(Request request);

  static const std::regex matchAPIRequest;
  static const std::regex matchLocalConfigRequest;
  static const std::regex matchLocalInfoRequest;

  private:
  Webserver* m_webserver;
  ConfigPtr m_config;
  Logger m_logger;
};
}
}

#endif
