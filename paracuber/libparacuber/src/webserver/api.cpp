#include "../../include/paracuber/webserver/api.hpp"
#include "../../include/paracuber/config.hpp"
#include "../../include/paracuber/webserver/webserver.hpp"
#include <regex>

namespace paracuber {
namespace webserver {
const std::regex API::matchAPIRequest("^/api/.*");
const std::regex API::matchLocalConfigRequest("^/api/local-config.json$");
const std::regex API::matchLocalInfoRequest("^/api/local-info.json$");

API::API(Webserver* webserver, ConfigPtr config, LogPtr log)
  : m_webserver(webserver)
  , m_logger(log->createLogger())
{}
API::~API() {}

bool
API::isAPIRequest(const std::string& target)
{
  return std::regex_match(target, matchAPIRequest);
}

API::Request
API::matchTargetToRequest(const std::string& target)
{
  if(std::regex_match(target, matchLocalInfoRequest))
    return LocalInfo;
  if(std::regex_match(target, matchLocalConfigRequest))
    return LocalConfig;
  return Unknown;
}

std::string
API::generateResponse(Request request)
{
  switch(request) {
    case LocalConfig:
      return "{\"hello\":\"config\"}";
    case LocalInfo:
      return "{\"hello\":\"info\"}";
    default:
      return "{\"success\":false}";
  }
}
}
}
