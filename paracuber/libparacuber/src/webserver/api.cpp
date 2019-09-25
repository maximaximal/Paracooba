#include "../../include/paracuber/webserver/api.hpp"
#include "../../include/paracuber/client.hpp"
#include "../../include/paracuber/communicator.hpp"
#include "../../include/paracuber/config.hpp"
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

void
API::injectCNFTreeNode(int64_t handle,
                       CNFTree::Path p,
                       CNFTree::CubeVar var,
                       CNFTree::StateEnum state)
{
  auto it = m_wsData.find(
    reinterpret_cast<const boost::asio::ip::tcp::socket*>(handle));
  if(it == m_wsData.end()) {
    PARACUBER_LOG(m_logger, LocalWarning)
      << "Invalid socket handle supplied to API in CNFTreeNode injection!";
    return;
  }
  WSData& data = it->second;

  handleInjectedCNFTreeNode(data, p, var, state);
}

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
    str += "  \"CNFVarCount\":" +
           std::to_string(m_config->getClient()->getCNFVarCount()) + "\n";
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
  auto it = m_wsData.find(socket);
  if(it == m_wsData.end()) {
    it = m_wsData.insert({ socket, { cb } }).first;
  }
  WSData& data = it->second;
  data.answer.clear();

  PARACUBER_LOG(m_logger, Trace) << "Receive WS Request";

  std::string type = ptree->get<std::string>("type");

  if(type == "cnftree-request-path") {
    // Request path.
    std::string strPath = ptree->get<std::string>("path");
    if(strPath.length() > CNFTree::maxPathDepth) {
      return sendError(data, "CNFTree path too long!");
    }
    CNFTree::Path p = CNFTree::strToPath(strPath.data(), strPath.length());
    m_config->getCommunicator()->requestCNFPathInfo(
      p, reinterpret_cast<int64_t>(socket));
  } else if(type == "ping") {
    // Ping
    data.answer.put("type", "pong");
    cb(data.answer);
  } else {
    return sendError(data, "Unknown message type \"" + type + "\"!");
  }
}

void
API::sendError(WSData& d, const std::string& str)
{
  d.answer.clear();
  d.answer.put("type", "error");
  d.answer.put("message", str);
  d.cb(d.answer);
}

void
API::handleInjectedCNFTreeNode(WSData& d,
                               CNFTree::Path p,
                               CNFTree::CubeVar var,
                               CNFTree::StateEnum state)
{
  char strPath[CNFTree::maxPathDepth + 1];
  CNFTree::pathToStr(p, strPath);
  auto& a = d.answer;
  a.clear();
  a.put("type", "cnftree-update");
  a.put("path", std::string(strPath, CNFTree::getDepth(p)));
  a.put("literal", var);
  a.put("state", CNFTreeStateToStr(state));
  PARACUBER_LOG(m_logger, Trace) << "Write WS Answer";
  d.cb(a);
  PARACUBER_LOG(m_logger, Trace) << "After Write WS Answer";
}
}
}
