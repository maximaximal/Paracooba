#ifndef PARACOOBA_NET_CONTROL
#define PARACOOBA_NET_CONTROL

#include <boost/asio/io_service.hpp>
#include <chrono>

#include "../log.hpp"
#include "../messages/message_receiver.hpp"

namespace paracooba {
class NetworkedNode;
class ClusterNodeStore;

namespace messages {
class Message;
class JobDescriptionReceiverProvider;
}

namespace net {
class Connection;
class TCPAcceptor;

class Control : public messages::MessageReceiver
{
  public:
  Control(boost::asio::io_service& ioService,
          ConfigPtr config,
          LogPtr log,
          ClusterNodeStore& clusterNodeStore);
  virtual ~Control();

  void setJobDescriptionReceiverProvider(
    messages::JobDescriptionReceiverProvider& jdRecProv)
  {
    m_jobDescriptionReceiverProvider = &jdRecProv;
  }

  virtual void receiveMessage(const messages::Message& msg, NetworkedNode& nn);

  void announceTo(NetworkedNode& nn);
  void requestAnnouncementFrom(NetworkedNode& nn);

  private:
  void handleOnlineAnnouncement(const messages::Message& msg,
                                NetworkedNode& conn);
  void handleOfflineAnnouncement(const messages::Message& msg,
                                 NetworkedNode& conn);
  void handleAnnouncementRequest(const messages::Message& msg,
                                 NetworkedNode& conn);
  void handleNodeStatus(const messages::Message& msg, NetworkedNode& conn);
  void handleCNFTreeNodeStatusRequest(const messages::Message& msg,
                                      NetworkedNode& conn);
  void handleCNFTreeNodeStatusReply(const messages::Message& msg,
                                    NetworkedNode& conn);
  void handleNewRemoteConnected(const messages::Message& msg,
                                NetworkedNode& conn);
  void handlePing(const messages::Message& msg, NetworkedNode& conn);
  void handlePong(const messages::Message& msg, NetworkedNode& conn);

  virtual void handlePingSent(ID id);

  private:

  void conditionallySetTracerOffset(ID id);
  void sendPing(NetworkedNode& conn, int64_t offset, bool daemon);

  boost::asio::io_service& m_ioService;
  ClusterNodeStore& m_clusterNodeStore;
  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  messages::JobDescriptionReceiverProvider* m_jobDescriptionReceiverProvider =
    nullptr;

  struct PingHandle
  {
    std::chrono::time_point<std::chrono::steady_clock> sent;
    std::chrono::time_point<std::chrono::steady_clock> answered;
    uint64_t pingTimeNs;
    int64_t offset = 0;
    bool setOffset = false;
    bool alreadySent = false;
    bool waitingForFullyKnown = false;
  };
  std::map<ID, PingHandle> m_pings;
};
}
}

#endif
