#include "../../include/paracuber/webserver/api.hpp"
#include "../../include/paracuber/config.hpp"
#include "../../include/paracuber/client.hpp"
#include "../../include/paracuber/webserver/webserver.hpp"
#include <cassert>
#include <regex>

namespace paracuber {
namespace webserver {
const std::regex API::matchAPIRequest("^/api/.*");
const std::regex API::matchLocalConfigRequest("^/api/local-config.json$");
const std::regex API::matchLocalInfoRequest("^/api/local-info.json$");

API::API(Webserver* webserver, ConfigPtr config, LogPtr log)
  : m_webserver(webserver)
  , m_logger(log->createLogger())
  , m_config(config)
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
  assert(m_config);
  assert(m_webserver);
  switch(request) {
    case LocalConfig:
      return generateResponseForLocalConfig();
    case LocalInfo:
      return generateResponseForLocalInfo();
    default:
      return "{\"success\":false}";
  }
}

std::string
API::generateResponseForLocalConfig()
{
  std::string str = "{\n";
  for(std::size_t i = 0; i < Config::_KEY_COUNT; ++i) {
    auto key = static_cast<Config::Key>(i);
    str += "  \"" + std::string(GetConfigNameFromEnum(key)) + "\":\"" +
           m_config->getKeyAsString(key) + "\"";
    if(i < Config::_KEY_COUNT - 1)
      str += ",";
    str += "\n";
  }
  str += "}";
  return str;
}
std::string
API::generateResponseForLocalInfo()
{
  std::string str = "{\n";
  if(!m_config->isDaemonMode()) {
    str += "  \"CNFVarCount\":" + std::to_string(m_config->getClient()->getCNFVarCount()) + "\n";
  }
  str += "}";
  return str;
}

void
API::handleWebSocketRequest(const boost::asio::ip::tcp::socket* socket,
                            WebSocketCB cb,
                            boost::property_tree::ptree* ptree)
{
  if(!ptree) {
    m_wsData.erase(socket);
    return;
  }
  WSData& data = m_wsData[socket];
  data.answer.clear();

  std::string type = ptree->get<std::string>("type");

  if(type == "cnftree-request-path") {
    // Request path.
  } else if(type == "ping") {
    // Ping
    data.answer.put("type", "pong");
  } else {
    data.answer.put("type", "error");
    data.answer.put("message", "Unknown message type: \"" + type + "\"!");
  }

  cb(data.answer);
}
}
}
