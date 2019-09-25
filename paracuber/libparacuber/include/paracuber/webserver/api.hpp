#ifndef PARACUBER_WEBSERVER_API_HPP
#define PARACUBER_WEBSERVER_API_HPP

#include "../cnftree.hpp"
#include "../log.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/property_tree/ptree.hpp>
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

  using WebSocketCB = std::function<void(boost::property_tree::ptree&)>;

  explicit API(Webserver* webserver, ConfigPtr config, LogPtr log);
  ~API();

  void injectCNFTreeNode(int64_t handle,
                         CNFTree::Path p,
                         CNFTree::CubeVar var,
                         CNFTree::StateEnum state);

  static bool isAPIRequest(const std::string& target);
  static Request matchTargetToRequest(const std::string& target);

  std::string generateResponse(Request request);

  std::string generateResponseForLocalConfig();
  std::string generateResponseForLocalInfo();

  void handleWebSocketRequest(const boost::asio::ip::tcp::socket* socket,
                              WebSocketCB cb,
                              boost::property_tree::ptree* ptree);

  struct WSData
  {
    WebSocketCB &cb;
    boost::property_tree::ptree answer;
  };
  std::map<const boost::asio::ip::tcp::socket*, WSData> m_wsData;

  void handleInjectedCNFTreeNode(WSData& d,
                                 CNFTree::Path p,
                                 CNFTree::CubeVar var,
                                 CNFTree::StateEnum state);

  void sendError(WSData &d, const std::string &str);

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
