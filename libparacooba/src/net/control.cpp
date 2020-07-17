#include "../../include/paracooba/net/control.hpp"
#include "../../include/paracooba/cluster-node-store.hpp"
#include "../../include/paracooba/cluster-node.hpp"
#include "../../include/paracooba/cnftree.hpp"
#include "../../include/paracooba/config.hpp"
#include "../../include/paracooba/daemon.hpp"
#include "../../include/paracooba/messages/message.hpp"
#include "../../include/paracooba/net/connection.hpp"
#include "../../include/paracooba/networked_node.hpp"

#include <chrono>
#include <regex>

#ifdef PARACOOBA_ENABLE_TRACING_SUPPORT
#include "../../include/paracooba/tracer.hpp"
#endif

namespace paracooba {
namespace net {
Control::Control(boost::asio::io_service& ioService,
                 ConfigPtr config,
                 LogPtr log,
                 ClusterNodeStore& clusterNodeStore)
  : m_ioService(ioService)
  , m_config(config)
  , m_log(log)
  , m_logger(log->createLogger("Control"))
  , m_clusterNodeStore(clusterNodeStore)
{}
Control::~Control() {}

void
Control::receiveMessage(const messages::Message& msg, NetworkedNode& nn)
{
  assert(m_jobDescriptionReceiverProvider);

  PARACOOBA_LOG(m_logger, NetTrace)
    << "  -> " << msg.getType() << " from " << nn;

  switch(msg.getType()) {
    case messages::Type::OnlineAnnouncement:
      handleOnlineAnnouncement(msg, nn);
      break;
    case messages::Type::OfflineAnnouncement:
      handleOfflineAnnouncement(msg, nn);
      break;
    case messages::Type::AnnouncementRequest:
      handleAnnouncementRequest(msg, nn);
      break;
    case messages::Type::NodeStatus:
      handleNodeStatus(msg, nn);
      break;
    case messages::Type::CNFTreeNodeStatusRequest:
      handleCNFTreeNodeStatusRequest(msg, nn);
      break;
    case messages::Type::CNFTreeNodeStatusReply:
      handleCNFTreeNodeStatusReply(msg, nn);
      break;
    case messages::Type::NewRemoteConnected:
      handleNewRemoteConnected(msg, nn);
      break;
    case messages::Type::Ping:
      handlePing(msg, nn);
      break;
    case messages::Type::Pong:
      handlePong(msg, nn);
      break;
    case messages::Type::Unknown:
      // Nothing to do on unknown messages.
      break;
  }
}

void
Control::announceTo(NetworkedNode& nn)
{
  nn.onlineAnnouncement(*m_config, nn);
}

void
Control::requestAnnouncementFrom(NetworkedNode& nn)
{
  nn.announcementRequest(*m_config, nn);
}

void
Control::handleOnlineAnnouncement(const messages::Message& msg,
                                  NetworkedNode& nn)

{
  const messages::OnlineAnnouncement& onlineAnnouncement =
    msg.getOnlineAnnouncement();
  const messages::Node& messageNode = onlineAnnouncement.getNode();

  // Always ping.
  sendPing(nn, messageNode.getTracerOffset(), messageNode.getDaemonMode());

  int64_t id = msg.getOrigin();

  auto [clusterNode, inserted] = m_clusterNodeStore.getOrCreateNode(id);

  clusterNode.applyOnlineAnnouncementMessage(onlineAnnouncement);
}
void
Control::handleOfflineAnnouncement(const messages::Message& msg,
                                   NetworkedNode& nn)
{
  const messages::OfflineAnnouncement& offlineAnnouncement =
    msg.getOfflineAnnouncement();

  m_clusterNodeStore.removeNode(msg.getOrigin(),
                                offlineAnnouncement.getReason());
}
void
Control::handleAnnouncementRequest(const messages::Message& msg,
                                   NetworkedNode& nn)
{
  const messages::AnnouncementRequest& announcementRequest =
    msg.getAnnouncementRequest();
  const messages::Node& requester = announcementRequest.getRequester();

  // Always ping.
  sendPing(nn, requester.getTracerOffset(), requester.getDaemonMode());

  int64_t id = msg.getOrigin();

  assert(msg.getOrigin() == requester.getId());

  auto [clusterNode, inserted] =
    m_clusterNodeStore.getOrCreateNode(msg.getOrigin());

  clusterNode.applyAnnouncementRequestMessage(announcementRequest);

  switch(announcementRequest.getNameMatchType()) {
    case messages::AnnouncementRequest::NameMatch::NO_RESTRICTION:
      break;
    case messages::AnnouncementRequest::NameMatch::REGEX:
      if(!std::regex_match(std::string(m_config->getString(Config::LocalName)),
                           std::regex(announcementRequest.getRegexMatch()))) {
        PARACOOBA_LOG(m_logger, Trace)
          << "No regex match! Regex: " << announcementRequest.getRegexMatch();
        return;
      }
      break;
    case messages::AnnouncementRequest::NameMatch::ID:
      if(m_config->getInt64(Config::Id) != announcementRequest.getIdMatch()) {
        return;
      }
  }

  if(requester.getId() != m_config->getInt64(Config::Id)) {
    announceTo(nn);
  }
}
void
Control::handleNodeStatus(const messages::Message& msg, NetworkedNode& nn)
{
  const messages::NodeStatus& nodeStatus = msg.getNodeStatus();

  int64_t id = msg.getOrigin();

  auto [clusterNode, inserted] = m_clusterNodeStore.getOrCreateNode(id);

  clusterNode.applyNodeStatusMessage(nodeStatus);

  if(!clusterNode.getFullyKnown()) {
    // Try the default target port as last hope to get to know the other node.
    NetworkedNode* nn = clusterNode.getNetworkedNode();
    nn->setUdpPort(m_config->getUint16(Config::UDPTargetPort));

    requestAnnouncementFrom(*nn);
  }
}
void
Control::handleCNFTreeNodeStatusRequest(const messages::Message& msg,
                                        NetworkedNode& nn)
{
  const messages::CNFTreeNodeStatusRequest& cnfTreeNodeStatusRequest =
    msg.getCNFTreeNodeStatusRequest();

  int64_t originId = msg.getOrigin();
  int64_t cnfId = cnfTreeNodeStatusRequest.getCnfId();
  Path path = cnfTreeNodeStatusRequest.getPath();

  CNF* cnf = nullptr;
  auto [clusterNode, inserted] = m_clusterNodeStore.getOrCreateNode(originId);

  if(m_config->isDaemonMode()) {
    auto [context, lock] = m_config->getDaemon()->getContext(cnfId);
    if(!context) {
      PARACOOBA_LOG(m_logger, GlobalWarning)
        << "CNFTreeNodeStatusRequest received before Context for ID " << cnfId
        << " existed!";
      return;
    }
    cnf = context->getRootCNF().get();
  }

  if(!cnf) {
    PARACOOBA_LOG(m_logger, LocalWarning)
      << "CNFTreeNodeStatusRequest received, but no CNF found for ID " << cnfId
      << "!";
    return;
  }
}
void
Control::handleCNFTreeNodeStatusReply(const messages::Message& msg,
                                      NetworkedNode& nn)
{
  const messages::CNFTreeNodeStatusReply& cnfTreeNodeStatusReply =
    msg.getCNFTreeNodeStatusReply();
  int64_t originId = msg.getOrigin();
  int64_t cnfId = cnfTreeNodeStatusReply.getCnfId();
  uint64_t path = cnfTreeNodeStatusReply.getPath();
  auto& handleStack = cnfTreeNodeStatusReply.getHandleStack();

  // Depending on the handle, this is sent to the internal CNFTree handling
  // mechanism (handle == 0) or to the webserver for viewing (handle != 0).

  auto nodes = cnfTreeNodeStatusReply.getNodes();

  if(nodes.size() == 0) {
    PARACOOBA_LOG(m_logger, GlobalWarning)
      << "Cannot parse CNFTreeNodeStatusReply message for path \""
      << CNFTree::pathToStrNoAlloc(path)
      << "\"! Does not contain any "
         "nodes.";
    return;
  }

  PARACOOBA_LOG(m_logger, Trace)
    << "Receive CNFTree info for path " << CNFTree::pathToStrNoAlloc(path);

  /*
  if(handleStack.size() == 1) {
    // This reply arrived at the original sender! Inject all available
    // information.
    for(auto& nodeIt : nodes) {
      m_communicator->injectCNFTreeNodeInfo(
        cnfId,
        handleStack.top(),
        nodeIt.path,
        static_cast<CNFTree::State>(nodeIt.state),
        originId);
    }
  } else {
    // Forward the reply to the next hop. Stack is implicitly popped.
    messages::Message replyMsg;
    messages::CNFTreeNodeStatusReply reply(cnfTreeNodeStatusReply);
    replyMsg.insertCNFTreeNodeStatusReply(std::move(reply));
    sendMessage(cnfTreeNodeStatusReply.getHandle(), replyMsg, false);
  }
  */
}
void
Control::handleNewRemoteConnected(const messages::Message& msg,
                                  NetworkedNode& nn)
{
  const messages::NewRemoteConnected& newRemoteConnected =
    msg.getNewRemoteConnected();
  int64_t originId = msg.getOrigin();

  auto [clusterNode, inserted] = m_clusterNodeStore.getOrCreateNode(originId);

  if(inserted) {
    PARACOOBA_LOG(m_logger, GlobalError)
      << "Received NewRemoteConnected message, but remote node was newly "
         "inserted. This should not happen!";
    return;
  }

  if(clusterNode.isDaemon()) {
    PARACOOBA_LOG(m_logger, GlobalError)
      << "Received NewRemoteConnected message from daemon node, but these "
         "messages should only be sent from Master nodes!";
    return;
  }

  if(m_clusterNodeStore.hasNode(newRemoteConnected.getId())) {
    PARACOOBA_LOG(m_logger, NetTrace)
      << "Received a NewRemoteConnected message for remote with id "
      << newRemoteConnected.getId()
      << ", but that remote is already in local cluster node store. Ignoring "
         "this message.";
    return;
  }

  PARACOOBA_LOG(m_logger, NetTrace)
    << "This NewRemoteConnected message concerns the remote "
    << newRemoteConnected.getRemote() << " with id "
    << newRemoteConnected.getId();

  assert(m_jobDescriptionReceiverProvider);

  // Connect to the new node. This should only happen once, but if it happens
  // multiple times, the Connection logic handles the issue.
  Connection connection(m_ioService,
                        m_log,
                        m_config,
                        m_clusterNodeStore,
                        *this,
                        *m_jobDescriptionReceiverProvider);
  connection.connect(newRemoteConnected.getRemote());
}

void
Control::handlePing(const messages::Message& msg, NetworkedNode& conn)
{
  messages::Message answer = conn.buildMessage(*m_config);
  answer.insert(messages::Pong(msg.getPing()));
  conn.transmitMessage(answer, conn);
}
void
Control::handlePong(const messages::Message& msg, NetworkedNode& conn)
{
  PingHandle& handle = m_pings[conn.getId()];
  int64_t pingTimeNs = (std::chrono::steady_clock::now() - handle.sent).count();

  PARACOOBA_LOG(m_logger, Trace)
    << "Ping to remote " << conn << " returned with delay of " << pingTimeNs
    << "ns";

#ifdef PARACOOBA_ENABLE_TRACING_SUPPORT
  if(handle.setOffset && m_config->isDaemonMode()) {
    // Reset tracer offset and automatically activate, if not yet active.
    if(!Tracer::get().isActive()) {
      Tracer::resetStart(handle.offset + pingTimeNs / 2);

      PARACOOBA_LOG(m_logger, Debug)
        << "Reset tracer start time to match connected client. Offset is "
        << handle.offset + pingTimeNs / 2 << "ns (Ping = " << pingTimeNs
        << "ns)";
    }
  }
#endif
  m_pings.erase(conn.getId());
}

void
Control::sendPing(NetworkedNode& conn, int64_t offset, bool daemon)
{
  PingHandle& handle = m_pings[conn.getId()];
  if(!handle.alreadySent) {
    handle.alreadySent = true;
    handle.sent = std::chrono::steady_clock::now();
    handle.offset = offset;
    handle.setOffset = !daemon;

    messages::Message pingMsg = conn.buildMessage(*m_config);
    pingMsg.insert(messages::Ping(conn.getId()));
    conn.transmitMessage(pingMsg, conn);
  }
}
}
}
