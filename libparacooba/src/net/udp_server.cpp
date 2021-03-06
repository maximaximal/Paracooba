#include "../../include/paracooba/net/udp_server.hpp"
#include "../../include/paracooba/cluster-node-store.hpp"
#include "../../include/paracooba/cluster-node.hpp"
#include "../../include/paracooba/config.hpp"
#include "../../include/paracooba/messages/message.hpp"
#include "../../include/paracooba/networked_node.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <cereal/archives/binary.hpp>
#include <mutex>

namespace paracooba {
namespace net {
#define REC_BUF_SIZE 4096

UDPServer::State::State(boost::asio::io_service& ioService,
                        boost::asio::ip::udp::endpoint endpoint,
                        boost::asio::ip::udp::endpoint broadcastEndpoint,
                        ConfigPtr config,
                        LogPtr log,
                        messages::MessageReceiver& messageReceiver)
  : socket(ioService, endpoint.protocol())
  , config(config)
  , broadcastEndpoint(broadcastEndpoint)
  , logger(log->createLogger("UDPServer"))
  , messageReceiver(messageReceiver)
{
  if(config->autoDiscoveryEnabled()) {
    socket.bind(endpoint);
    socket.set_option(boost::asio::socket_base::broadcast(true));
  }
}

UDPServer::State::~State()
{
  if(config->autoDiscoveryEnabled()) {
    PARACOOBA_LOG(logger, Trace)
      << "UDPServer at " << socket.local_endpoint() << ":"
      << " stopped.";
  }
}

UDPServer::UDPServer(boost::asio::io_service& ioService,
                     boost::asio::ip::udp::endpoint endpoint,
                     boost::asio::ip::udp::endpoint broadcastEndpoint,
                     ConfigPtr config,
                     LogPtr log,
                     messages::MessageReceiver& messageReceiver)
  : m_state(std::make_shared<State>(ioService,
                                    endpoint,
                                    broadcastEndpoint,
                                    config,
                                    log,
                                    messageReceiver))
{}
UDPServer::~UDPServer() {}

void
UDPServer::startAccepting(ClusterNodeStore& clusterNodeStore,
                          ClusterNode& thisNode)
{
  assert(config()->autoDiscoveryEnabled());

  m_state->clusterNodeStore = &clusterNodeStore;
  m_state->thisNode = &thisNode;

  auto endpoint = socket().local_endpoint();
  NetworkedNode* nn = thisNode.getNetworkedNode();
  assert(nn);
  nn->setRemoteUdpEndpoint(endpoint);
  accept();
  PARACOOBA_LOG(logger(), Debug)
    << "UDPServer started at " << endpoint << " with broadcast endpoint "
    << broadcastEndpoint();
}

void
UDPServer::accept()
{
  assert(config()->autoDiscoveryEnabled());
  recvStreambuf().consume(recvStreambuf().size() + 1);
  (*this)(boost::system::error_code(), 0);
}

#include <boost/asio/yield.hpp>

void
UDPServer::operator()(const boost::system::error_code& ec,
                      size_t bytes_received)
{
  assert(config()->autoDiscoveryEnabled());
  enrichLogger();

  if(!ec || ec == boost::asio::error::message_size) {
    reenter(readCoro())
    {
      for(;;) {
        yield socket().async_receive_from(
          recvStreambuf().prepare(REC_BUF_SIZE), remoteEndpoint(), *this);

        PARACOOBA_LOG(logger(), NetTrace)
          << "Receive message of " << bytes_received << " bytes from "
          << remoteEndpoint() << ". Trying to decode.";

        try {
          messages::Message msg;
          recvStreambuf().commit(bytes_received);
          std::istream recvIstream(&recvStreambuf());
          cereal::BinaryInputArchive iarchive(recvIstream);
          iarchive(msg);

          auto [node, inserted] =
            clusterNodeStore().getOrCreateNode(msg.getOrigin());
          NetworkedNode* nn = node.getNetworkedNode();
          nn->setRemoteUdpEndpoint(remoteEndpoint());
          nn->setRemoteTcpEndpoint(boost::asio::ip::tcp::endpoint(
            remoteEndpoint().address(),
            config()->getUint16(Config::TCPTargetPort)));

          messageReceiver().receiveMessage(msg, *nn);
        } catch(cereal::Exception& e) {
          PARACOOBA_LOG(logger(), GlobalError)
            << "Received invalid message, parsing threw serialisation "
               "exception! "
               "Message: "
            << e.what();
        }

        recvStreambuf().consume(recvStreambuf().size() + 1);
      }
    }
  } else {
    PARACOOBA_LOG(logger(), LocalError)
      << "Error receiving data from UDP socket. Error: " << ec.message();
    readCoro() = boost::asio::coroutine();
    accept();
  }
}

#include <boost/asio/unyield.hpp>

void
UDPServer::transmitMessage(const messages::Message& msg,
                           NetworkedNode& nn,
                           SuccessCB successCB)
{
  if(!config()->autoDiscoveryEnabled()) {
    PARACOOBA_LOG(logger(), LocalError)
      << "Cannot send from UDP Server when UDP is disabled!";
    return;
  }

  if(!nn.isUdpPortSet() || !nn.isUdpEndpointSet()) {
    if(successCB) {
      successCB(false);
    }
    return;
  }
  assert(nn.isUdpEndpointSet());
  assert(nn.isUdpPortSet());
  transmitMessageToEndpoint(msg, nn.getRemoteUdpEndpoint(), successCB);
}

void
UDPServer::broadcastMessage(const messages::Message& msg, SuccessCB successCB)
{
  if(!config()->autoDiscoveryEnabled()) {
    PARACOOBA_LOG(logger(), LocalError)
      << "Cannot broadcast from UDP Server when UDP is disabled!";
    return;
  }
  transmitMessageToEndpoint(msg, broadcastEndpoint(), successCB);
}

void
UDPServer::transmitMessageToEndpoint(const messages::Message& msg,
                                     boost::asio::ip::udp::endpoint target,
                                     SuccessCB sendFinishedCB)
{
  if(!config()->autoDiscoveryEnabled()) {
    PARACOOBA_LOG(logger(), LocalError)
      << "Cannot transmit message to endpoint from UDP Server when UDP is "
         "disabled!";
    return;
  }

  std::lock_guard lock(sendMutex());
  std::ostream sendOstream(&sendStreambuf());
  cereal::BinaryOutputArchive oarchive(sendOstream);
  oarchive(msg);

  size_t bytes;
  size_t bytesToSend = sendStreambuf().size();
  bool success = true;

  PARACOOBA_LOG(logger(), NetTrace)
    << "Transmit ControlMessage of type " << msg.getType() << " with size "
    << bytesToSend << " to " << target;

  try {
    bytes = socket().send_to(sendStreambuf().data(), target);
  } catch(const std::exception& e) {
    PARACOOBA_LOG(logger(), LocalError)
      << "Exception encountered when sending message to endpoint " << target
      << "! Message: " << e.what();
    success = false;
  }
  if(bytes != bytesToSend) {
    PARACOOBA_LOG(logger(), LocalError) << "Only (synchronously) sent " << bytes
                                        << " of target " << bytesToSend << "!";
    success = false;
  }
  sendStreambuf().consume(sendStreambuf().size() + 1);
  if(sendFinishedCB) {
    sendFinishedCB(success);
  }
}
void
UDPServer::enrichLogger()
{
  if(!logger().log->isLogLevelEnabled(Log::NetTrace))
    return;

  std::stringstream m;
  m << "{";
  m << "R:'" << remoteEndpoint() << "'";
  m << "}";

  logger().setMeta(m.str());
}
}
}
