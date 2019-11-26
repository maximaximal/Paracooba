#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/client.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/cuber/registry.hpp"
#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/networked_node.hpp"
#include "../include/paracuber/runner.hpp"
#include <boost/asio.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/bind.hpp>
#include <cassert>
#include <chrono>
#include <mutex>
#include <regex>

#ifdef ENABLE_INTERNAL_WEBSERVER
#include "../include/paracuber/webserver/api.hpp"
#endif

#include <capnp-schemas/message.capnp.h>
#include <capnp/serialize.h>

using boost::asio::ip::udp;
using namespace paracuber::message;

#define REC_BUF_SIZE 4096

namespace paracuber {
void
applyMessageNodeToStatsNode(const message::Node::Reader& src,
                            ClusterStatistics::Node& tgt)
{
  tgt.setName(src.getName());
  tgt.setId(src.getId());
  tgt.setMaximumCPUFrequency(src.getMaximumCPUFrequency());
  tgt.setAvailableWorkers(src.getAvailableWorkers());
  tgt.setUptime(src.getUptime());
  tgt.setWorkQueueCapacity(src.getWorkQueueCapacity());
  tgt.setWorkQueueSize(src.getWorkQueueSize());
  tgt.setUdpListenPort(src.getUdpListenPort());
  tgt.setTcpListenPort(src.getTcpListenPort());
  tgt.setFullyKnown(true);
  tgt.setDaemon(src.getDaemonMode());
}
void
applyMessageNodeStatusToStatsNode(const message::NodeStatus::Reader& src,
                                  ClusterStatistics::Node& tgt,
                                  Config& config)
{
  tgt.setWorkQueueSize(src.getWorkQueueSize());

  if(src.isDaemon()) {
    auto daemon = src.getDaemon();

    for(auto context : daemon.getContexts()) {
      tgt.setContextStateAndSize(
        context.getOriginator(), context.getState(), context.getFactorySize());
    }
  }
}

class Communicator::UDPServer
{
  public:
  UDPServer(Communicator* comm,
            LogPtr log,
            boost::asio::io_service& ioService,
            uint16_t port)
    : m_communicator(comm)
    , m_socket(ioService, udp::endpoint(udp::v4(), port))
    , m_logger(log->createLogger())
    , m_port(port)
    , m_clusterStatistics(comm->getClusterStatistics())
  {
    startReceive();
    PARACUBER_LOG(m_logger, Trace) << "UDPServer started at port " << port;
  }
  ~UDPServer()
  {
    PARACUBER_LOG(m_logger, Trace)
      << "UDPServer at port " << m_port << " stopped.";
  }

  void startReceive()
  {
    m_socket.set_option(boost::asio::socket_base::broadcast(true));

    m_socket.async_receive_from(
      boost::asio::buffer(reinterpret_cast<std::byte*>(&m_recBuf[0]),
                          m_recBuf.size() * sizeof(::capnp::word)),
      m_remoteEndpoint,
      boost::bind(&UDPServer::handleReceive,
                  this,
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
  }

  void handleReceive(const boost::system::error_code& error, std::size_t bytes)
  {
    // No framing required. A single receive always maps directly to a message.
    if(!error || error == boost::asio::error::message_size) {
      // Alignment handled by the creation of m_recBuf.

      ::capnp::FlatArrayMessageReader reader(
        kj::arrayPtr(reinterpret_cast<::capnp::word*>(m_recBuf.begin()),
                     bytes / sizeof(::capnp::word)));

      Message::Reader msg = reader.getRoot<Message>();
      switch(msg.which()) {
        case Message::ONLINE_ANNOUNCEMENT:
          PARACUBER_LOG(m_logger, Trace)
            << "  -> Online Announcement from " << m_remoteEndpoint
            << " (ID: " << msg.getOrigin() << ")";
          handleOnlineAnnouncement(msg);
          break;
        case Message::OFFLINE_ANNOUNCEMENT:
          PARACUBER_LOG(m_logger, Trace)
            << "  -> Offline Announcement from " << m_remoteEndpoint
            << " (ID: " << msg.getOrigin() << ")";
          handleOfflineAnnouncement(msg);
          break;
        case Message::ANNOUNCEMENT_REQUEST:
          PARACUBER_LOG(m_logger, Trace)
            << "  -> Announcement Request from " << m_remoteEndpoint
            << " (ID: " << msg.getOrigin() << ")";
          handleAnnouncementRequest(msg);
          break;
        case Message::NODE_STATUS:
          /*
          // These logging messages are not required most of the time and only
          // waste processor time.

          PARACUBER_LOG(m_logger, Trace)
            << "  -> Node Status from " << m_remoteEndpoint
            << " (ID: " << msg.getOrigin() << ")";
          */
          handleNodeStatus(msg);
          break;
        case Message::CNF_TREE_NODE_STATUS_REQUEST:
          PARACUBER_LOG(m_logger, Trace)
            << "  -> CNFTree Node Status Request from " << m_remoteEndpoint
            << " (ID: " << msg.getOrigin() << ")";
          handleCNFTreeNodeStatusRequest(msg);
          break;
        case Message::CNF_TREE_NODE_STATUS_REPLY:
          PARACUBER_LOG(m_logger, Trace)
            << "  -> CNFTree Node Status Reply from " << m_remoteEndpoint
            << " (ID: " << msg.getOrigin() << ")";
          handleCNFTreeNodeStatusReply(msg);
          break;
      }

      startReceive();
    } else {
      PARACUBER_LOG(m_logger, LocalError)
        << "Error receiving data from UDP socket. Error: " << error;
    }
  }

  void networkStatisticsNode(ClusterStatistics::Node& node)
  {
    NetworkedNode* nn = node.getNetworkedNode();
    if(!nn) {
      std::unique_ptr<NetworkedNode> networkedNode =
        std::make_unique<NetworkedNode>(m_remoteEndpoint, node.getId());
      networkedNode->setUdpPort(node.getUdpListenPort());
      networkedNode->setTcpPort(node.getTcpListenPort());
      node.setNetworkedNode(std::move(networkedNode));
      nn = node.getNetworkedNode();
    }

    if(!node.getFullyKnown() &&
       node.getId() != m_communicator->m_config->getInt64(Config::Id)) {
      m_communicator->m_ioService.post(
        std::bind(&Communicator::task_requestAnnounce,
                  m_communicator,
                  node.getId(),
                  "",
                  nn));
    }
  }

  void handleOnlineAnnouncement(Message::Reader& msg)
  {
    const OnlineAnnouncement::Reader& reader = msg.getOnlineAnnouncement();
    const Node::Reader& messageNode = reader.getNode();

    int64_t id = msg.getOrigin();

    auto [statisticsNode, inserted] = m_clusterStatistics->getOrCreateNode(id);

    applyMessageNodeToStatsNode(messageNode, statisticsNode);

    networkStatisticsNode(statisticsNode);

    if(inserted && !m_communicator->m_config->isDaemonMode()) {
      // Send announcement of this client to the other node before transferring
      // the root CNF.
      m_communicator->m_ioService.post(
        std::bind(&Communicator::task_announce,
                  m_communicator,
                  statisticsNode.getNetworkedNode()));

      m_communicator->sendCNFToNode(
        m_communicator->m_config->getClient()->getRootCNF(),
        0,// Send root node only.
        statisticsNode.getNetworkedNode());
    }

    PARACUBER_LOG(m_logger, Info) << *m_clusterStatistics;
    m_communicator->checkAndTransmitClusterStatisticsChanges();
  }
  void handleOfflineAnnouncement(Message::Reader& msg)
  {
    const OfflineAnnouncement::Reader& reader = msg.getOfflineAnnouncement();

    m_clusterStatistics->removeNode(msg.getOrigin());

    PARACUBER_LOG(m_logger, Info) << *m_clusterStatistics;
    m_communicator->checkAndTransmitClusterStatisticsChanges();
  }
  void handleAnnouncementRequest(Message::Reader& msg)
  {
    const AnnouncementRequest::Reader& reader = msg.getAnnouncementRequest();
    const Node::Reader& requester = reader.getRequester();

    auto [statisticsNode, inserted] =
      m_clusterStatistics->getOrCreateNode(requester.getId());

    applyMessageNodeToStatsNode(requester, statisticsNode);

    networkStatisticsNode(statisticsNode);

    PARACUBER_LOG(m_logger, Info) << *m_clusterStatistics;

    if(!m_communicator->m_config->isDaemonMode()) {
      if(requester.getDaemonMode()) {
        m_communicator->sendCNFToNode(
          m_communicator->m_config->getClient()->getRootCNF(),
          0,// Send the root node only, no path required yet at this stage.
          statisticsNode.getNetworkedNode());
      }
    }

    auto nameMatch = reader.getNameMatch();
    switch(nameMatch.which()) {
      case AnnouncementRequest::NameMatch::NO_RESTRICTION:
        break;
      case AnnouncementRequest::NameMatch::REGEX:
        if(!std::regex_match(std::string(m_communicator->m_config->getString(
                               Config::LocalName)),
                             std::regex(nameMatch.getRegex().cStr()))) {
          PARACUBER_LOG(m_logger, Trace)
            << "No regex match! Regex: " << nameMatch.getRegex().cStr();
          return;
        }
        break;
      case AnnouncementRequest::NameMatch::ID:
        if(!m_communicator->m_config->getInt64(Config::Id) ==
           nameMatch.getId()) {
          return;
        }
    }

    if(requester.getId() != m_communicator->m_config->getInt64(Config::Id)) {
      m_communicator->m_ioService.post(
        std::bind(&Communicator::task_announce,
                  m_communicator,
                  statisticsNode.getNetworkedNode()));
    }
  }
  void handleNodeStatus(Message::Reader& msg)
  {
    const NodeStatus::Reader& reader = msg.getNodeStatus();

    int64_t id = msg.getOrigin();

    auto [statisticsNode, inserted] = m_clusterStatistics->getOrCreateNode(id);

    applyMessageNodeStatusToStatsNode(
      reader, statisticsNode, *m_communicator->m_config);

    networkStatisticsNode(statisticsNode);

    m_communicator->checkAndTransmitClusterStatisticsChanges();
  }

  void handleCNFTreeNodeStatusRequest(Message::Reader& msg)
  {
    const CNFTreeNodeStatusRequest::Reader& reader =
      msg.getCnfTreeNodeStatusRequest();
    int64_t originId = msg.getOrigin();
    int64_t cnfId = reader.getCnfId();
    CNFTree::Path path = reader.getPath();

    CNF* cnf = nullptr;
    auto [statisticsNode, inserted] =
      m_clusterStatistics->getOrCreateNode(originId);
    networkStatisticsNode(statisticsNode);

    if(m_communicator->m_config->isDaemonMode()) {
      auto [context, lock] =
        m_communicator->m_config->getDaemon()->getContext(cnfId);
      if(!context) {
        PARACUBER_LOG(m_logger, GlobalWarning)
          << "CNFTreeNodeStatusRequest received before Context for ID " << cnfId
          << " existed!";
        return;
      }
      cnf = context->getRootCNF().get();
    }

    if(!cnf) {
      PARACUBER_LOG(m_logger, LocalWarning)
        << "CNFTreeNodeStatusRequest received, but no CNF found for ID "
        << cnfId << "!";
      return;
    }

    // Build answer message.
    auto msgBuilder = getMessageBuilder();
    auto reply = msgBuilder.initCnfTreeNodeStatusReply();
    reply.setCnfId(cnfId);
    reply.setHandle(reader.getHandle());
    reply.setPath(reader.getPath());

    std::vector<std::pair<CNFTree::State, CNFTree::CubeVar>> entries;

    cnf->getCNFTree().visit(reader.getPath(),
                            [this, path, &entries, &msg](CNFTree::CubeVar var,
                                                         uint8_t depth,
                                                         CNFTree::State state,
                                                         int64_t remote) {
                              var = FastAbsolute(var);
                              entries.push_back({ state, var });
                              return false;
                            });

    if(entries.size() != CNFTree::getDepth(path) + 1) {
      // Not enough local information for a reply.
      PARACUBER_LOG(m_logger, LocalError)
        << "No reply to request for path " << CNFTree::pathToStrNoAlloc(path);
      return;
    }

    auto nodes = reply.initNodes(entries.size());
    auto it = nodes.begin();
    for(auto& entry : entries) {
      it->setState(entry.first);
      it->setLiteral(entry.second);
      ++it;
    }

    sendBuiltMessage(m_remoteEndpoint, false);
  }
  void handleCNFTreeNodeStatusReply(Message::Reader& msg)
  {
    const CNFTreeNodeStatusReply::Reader& reader =
      msg.getCnfTreeNodeStatusReply();
    int64_t originId = msg.getOrigin();
    int64_t cnfId = reader.getCnfId();
    int64_t handle = reader.getHandle();
    uint64_t path = reader.getPath();

    // Depending on the handle, this is sent to the internal CNFTree handling
    // mechanism (handle == 0) or to the webserver for viewing (handle != 0).

    auto nodes = reader.getNodes();

    if(nodes.size() == 0) {
      PARACUBER_LOG(m_logger, GlobalWarning)
        << "Cannot parse CNFTreeNodeStatusReply message for path \""
        << CNFTree::pathToStrNoAlloc(path)
        << "\"! Does not contain any "
           "nodes.";
      return;
    }

    PARACUBER_LOG(m_logger, Trace)
      << "Receive CNFTree info for path " << CNFTree::pathToStrNoAlloc(path);

    if(handle == 0) {

    } else {
      // Only the very last entry is required for viewing. The rest can be used
      // for load balancing purposes.

      auto lastEntry = nodes.begin() + (nodes.size() - 1);
      m_communicator->injectCNFTreeNodeInfo(
        cnfId,
        handle,
        path,
        lastEntry->getLiteral(),
        static_cast<CNFTree::StateEnum>(lastEntry->getState()),
        originId);
    }
  }

  void handleSend(const boost::system::error_code& error,
                  std::size_t bytes,
                  std::mutex* sendBufMutex)
  {
    sendBufMutex->unlock();

    if(!error || error == boost::asio::error::message_size) {
    } else {
      PARACUBER_LOG(m_logger, LocalError)
        << "Error sending data from UDP socket. Error: " << error.message();
    }
  }

  inline ::capnp::MallocMessageBuilder& getMallocMessageBuilder()
  {
    // This mechanism destructs the message builder, so it can be reconstructed
    // afterwards. This should be able to be improved at a later stage, but
    // could require deeper modifications of Capn'Proto.
    m_mallocMessageBuilder.~MallocMessageBuilder();
    new(&m_mallocMessageBuilder)::capnp::MallocMessageBuilder(m_sendBuf);
    return m_mallocMessageBuilder;
  }
  Message::Builder getMessageBuilder()
  {
    auto msg = getMallocMessageBuilder().getRoot<Message>();
    msg.setOrigin(m_communicator->m_config->getInt64(Config::Id));
    msg.setId(m_communicator->getAndIncrementCurrentMessageId());
    return std::move(msg);
  }
  void sendBuiltMessage(boost::asio::ip::udp::endpoint endpoint,
                        bool fromRunner = true,
                        bool async = true)
  {
    // One copy cannot be avoided.
    auto buf = std::move(::capnp::messageToFlatArray(m_mallocMessageBuilder));
    size_t size = ::capnp::computeSerializedSizeInWords(m_mallocMessageBuilder);

    {
      // Lock the mutex when sending and unlock it when the data has been sent
      // in the handleSend function. This way, async sending is still fast when
      // calculating stuff and does not block, but it DOES block when more
      // messages are sent.
      //
      // This is only required when sending from a runner thread, which is the
      // default. Messages sent from boost asio do not need this synchronisation
      // safe-guard.
      if(fromRunner) {
        m_sendBufMutex.lock();
      }

      auto bufBytes = buf.asBytes();
      std::copy(bufBytes.begin(), bufBytes.end(), m_sendBufBytes.begin());

      if(async) {
        m_socket.async_send_to(
          boost::asio::buffer(reinterpret_cast<std::byte*>(&m_sendBufBytes[0]),
                              size * sizeof(::capnp::word)),
          endpoint,
          boost::bind(&UDPServer::handleSend,
                      this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred,
                      &m_sendBufMutex));
      } else {
        m_socket.send_to(
          boost::asio::buffer(reinterpret_cast<std::byte*>(&m_sendBufBytes[0]),
                              size * sizeof(::capnp::word)),
          endpoint);

        if(fromRunner) {
          m_sendBufMutex.unlock();
        }
      }
    }
  }

  private:
  Communicator* m_communicator;
  Logger m_logger;
  uint16_t m_port;

  udp::socket m_socket;
  udp::endpoint m_remoteEndpoint;
  boost::array<::capnp::word, REC_BUF_SIZE / sizeof(::capnp::word)> m_recBuf;

  ClusterStatisticsPtr m_clusterStatistics;

  static thread_local kj::FixedArray<::capnp::word,
                                     REC_BUF_SIZE / sizeof(::capnp::word)>
    m_sendBuf;
  static thread_local boost::array<unsigned char, REC_BUF_SIZE> m_sendBufBytes;
  static thread_local ::capnp::MallocMessageBuilder m_mallocMessageBuilder;
  static thread_local std::mutex m_sendBufMutex;
};

thread_local kj::FixedArray<::capnp::word, REC_BUF_SIZE / sizeof(::capnp::word)>
  Communicator::UDPServer::m_sendBuf;
thread_local boost::array<unsigned char, REC_BUF_SIZE>
  Communicator::UDPServer::m_sendBufBytes;
thread_local std::mutex Communicator::UDPServer::m_sendBufMutex;
thread_local ::capnp::MallocMessageBuilder
  Communicator::UDPServer::m_mallocMessageBuilder;

class Communicator::TCPClient : public std::enable_shared_from_this<TCPClient>
{
  public:
  TCPClient(Communicator* comm,
            LogPtr log,
            boost::asio::io_service& ioService,
            boost::asio::ip::tcp::endpoint endpoint,
            TCPClientMode mode,
            std::shared_ptr<CNF> cnf,
            CNFTree::Path path = 0)
    : m_comm(comm)
    , m_cnf(cnf)
    , m_mode(mode)
    , m_endpoint(endpoint)
    , m_socket(ioService)
    , m_path(path)
  {
    PARACUBER_LOG(m_logger, Trace)
      << "Start TCPClient to " << m_endpoint << " for path "
      << CNFTree::pathToStrNoAlloc(path) << " with mode " << mode;
  }
  virtual ~TCPClient()
  {
    PARACUBER_LOG(m_logger, Trace) << "Destroy TCPClient to " << m_endpoint;
  }

  void connect()
  {
    m_socket.async_connect(m_endpoint,
                           boost::bind(&TCPClient::handleConnect,
                                       shared_from_this(),
                                       boost::asio::placeholders::error));
  }

  void handleConnect(const boost::system::error_code& err)
  {
    if(err) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not connect to remote endpoint " << m_endpoint
        << " over TCP! Error: " << err;
      return;
    }

    // Optimizes TCP sending on Linux by waiting for more data to arrive before
    // sending.
    int option = 1;
    setsockopt(m_socket.native_handle(),
               SOL_TCP,
               TCP_CORK,
               (char*)&option,
               sizeof(option));

    int64_t id = 0;
    if(m_comm->m_config->isDaemonMode()) {
      id = m_cnf->getOriginId();
    } else {
      id = m_comm->m_config->getInt64(Config::Id);
    };

    m_socket.write_some(
      boost::asio::buffer(reinterpret_cast<int64_t*>(&id), sizeof(int64_t)));

    auto ptr = shared_from_this();

    m_socket.native_non_blocking(true);

    // The transmission type handling is done inside the CNF sending functions,
    // receiving is therefore completely handled inside the CNF class. Only the
    // ID is handled outside to be able to match the connection to the CNF on
    // the other side.
    switch(m_mode) {
      case TCPClientMode::TransmitCNF:
        // Let the sending be handled by the CNF itself.
        m_cnf->send(&m_socket, m_path, [this, ptr]() {
          // Finished sending CNF!
        });
        break;
      case TCPClientMode::TransmitAllowanceMap:
        // Let the sending be handled by the CNF itself.
        m_cnf->sendAllowanceMap(&m_socket, [this, ptr]() {
          // Finished sending AllowanceMap!
        });
        break;
      case TCPClientMode::TransmitCNFResult:
        m_cnf->sendResult(&m_socket, m_path, [this, ptr]() {
          // Finished sending Result!
        });
        break;
    }
  }

  ReadyWaiter<boost::asio::ip::tcp::socket> socketWaiter;

  private:
  LoggerMT m_logger;
  Communicator* m_comm;
  std::shared_ptr<CNF> m_cnf;
  CNFTree::Path m_path;
  boost::asio::ip::tcp::socket m_socket;
  boost::asio::ip::tcp::endpoint m_endpoint;
  boost::array<char, REC_BUF_SIZE> m_recBuffer;
  TCPClientMode m_mode;
};

class Communicator::TCPServer
{
  public:
  TCPServer(Communicator* comm,
            LogPtr log,
            boost::asio::io_service& ioService,
            uint16_t port)
    : m_communicator(comm)
    , m_logger(log->createLogger())
    , m_log(log)
    , m_port(port)
    , m_ioService(ioService)
    , m_acceptor(
        ioService,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
  {
    startAccept();
    PARACUBER_LOG(m_logger, Trace) << "TCPServer started at port " << port;
  }
  virtual ~TCPServer()
  {
    PARACUBER_LOG(m_logger, Trace)
      << "TCPServer at port " << m_port << " stopped.";
  }

  class TCPServerClient : public std::enable_shared_from_this<TCPServerClient>
  {
    public:
    TCPServerClient(boost::asio::io_service& ioService,
                    LogPtr log,
                    Communicator* comm)
      : m_ioService(ioService)
      , m_socket(ioService)
      , m_logger(log->createLogger())
      , m_comm(comm)
    {}
    virtual ~TCPServerClient() {}

    enum State
    {
      NewConnection,
      Receiving,
    };

    boost::asio::ip::tcp::socket& socket() { return m_socket; }

    void start() { startReceive(); }

    void startReceive()
    {
      using namespace boost::asio;
      m_socket.async_receive(buffer(&m_recBuffer[0], m_recBuffer.size()),
                             boost::bind(&TCPServerClient::handleReceive,
                                         shared_from_this(),
                                         placeholders::error,
                                         placeholders::bytes_transferred));
    }

    void handleReceive(const boost::system::error_code& error,
                       std::size_t bytes)
    {
      if(error == boost::asio::error::eof) {
        switch(m_state) {
          case NewConnection:
            break;
          case Receiving:
            CNF::TransmissionSubject subject =
              m_cnf->receive(&m_socket, nullptr, 0);
            if(m_context) {
              Daemon::Context::State change;
              switch(subject) {
                case CNF::TransmitFormula:
                  change = Daemon::Context::FormulaReceived;
                  break;
                case CNF::TransmitAllowanceMap:
                  change = Daemon::Context::AllowanceMapReceived;
                  break;
                case CNF::TransmitResult:
                  change = Daemon::Context::ResultReceived;
                  break;
                default:
                  change = Daemon::Context::JustCreated;
                  break;
              }
              m_context->start(change);
            }
            break;
        }
        PARACUBER_LOG(m_logger, Trace)
          << "TCP Connection ended from " << m_socket.remote_endpoint();
        return;
      }
      if(error) {
        PARACUBER_LOG(m_logger, LocalError)
          << "Could read from TCP connection! Error: " << error.message();
        return;
      }
      m_pos = 0;

      switch(m_state) {
        case NewConnection: {
          state_newConnection(bytes);
          break;
        }
        case Receiving: {
          state_receiving(bytes);
          break;
        }
      }

      startReceive();
    }

    private:
    void state_newConnection(std::size_t bytes)
    {
      // If the connection is new, this client must be initialised first. A
      // connection has a header containing its type and assorted data. This
      // is extracted in this stage (first few bytes of the TCP stream).
      int64_t originator = *reinterpret_cast<int64_t*>(&m_recBuffer[m_pos]);
      m_pos += sizeof(int64_t);

      if(m_comm->m_config->isDaemonMode()) {
        auto [context, inserted] =
          m_comm->m_config->getDaemon()->getOrCreateContext(originator);
        m_context = &context;
        m_cnf = m_context->getRootCNF();
      } else {
        m_cnf = m_comm->m_config->getClient()->getRootCNF();
      }

      m_state = Receiving;

      state_receiving(bytes - m_pos);
    }
    void state_receiving(std::size_t bytes)
    {
      // The receiving file state exists to shuffle data from the network
      // stream into the locally created file. After finishing, the created
      // file can be given to a new task to be solved.

      if(bytes == 0) {
        // This can happen in the initial request.
        return;
      }

      m_cnf->receive(&m_socket, &m_recBuffer[m_pos], bytes);
    }

    std::shared_ptr<CNF> m_cnf;

    boost::asio::io_service& m_ioService;
    boost::asio::ip::tcp::socket m_socket;
    boost::array<char, REC_BUF_SIZE> m_recBuffer;
    std::size_t m_pos = 0;
    Logger m_logger;
    Communicator* m_comm;
    State m_state = NewConnection;
    Daemon::Context* m_context = nullptr;
  };

  void startAccept()
  {
    std::shared_ptr<TCPServerClient> newConn =
      std::make_shared<TCPServerClient>(m_ioService, m_log, m_communicator);

    m_acceptor.async_accept(newConn->socket(),
                            boost::bind(&TCPServer::handleAccept,
                                        this,
                                        newConn,
                                        boost::asio::placeholders::error()));
  }
  void handleAccept(std::shared_ptr<TCPServerClient> newConn,
                    const boost::system::error_code& error)
  {
    if(error) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not accept new TCP connection! Error: " << error.message();
      return;
    }
    newConn->socket().non_blocking(true);
    newConn->start();

    PARACUBER_LOG(m_logger, Trace) << "New TCP connection started!";

    startAccept();
  }

  private:
  Communicator* m_communicator;
  Logger m_logger;
  LogPtr m_log;
  uint16_t m_port;
  boost::asio::ip::tcp::acceptor m_acceptor;
  boost::asio::io_service& m_ioService;
};

Communicator::Communicator(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_log(log)
  , m_ioServiceWork(m_ioService)
  , m_logger(log->createLogger())
  , m_signalSet(std::make_unique<boost::asio::signal_set>(m_ioService, SIGINT))
  , m_clusterStatistics(std::make_shared<ClusterStatistics>(config, log))
  , m_tickTimer(
      (m_ioService),
      std::chrono::milliseconds(m_config->getUint64(Config::TickMilliseconds)))
{
  m_config->m_communicator = this;
  m_signalSet->async_wait(std::bind(&Communicator::signalHandler,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));

  if(config->isInternalWebserverEnabled()) {
    m_webserverInitiator =
      std::make_unique<webserver::Initiator>(config, log, m_ioService);
  }
}

Communicator::~Communicator()
{
  m_config->m_communicator = nullptr;
  exit();
  PARACUBER_LOG(m_logger, Trace) << "Destruct Communicator.";
}

void
Communicator::run()
{
  using namespace boost::asio;

  // First, init the local node in cluster statistics with all set variables.
  m_clusterStatistics->initLocalNode();

  if(!m_runner) {
    m_runner = std::make_shared<Runner>(this, m_config, m_log);
  }
  if(!m_runner->isRunning()) {
    m_runner->start();
  }

  listenForIncomingUDP(m_config->getUint16(Config::UDPListenPort));
  listenForIncomingTCP(m_config->getUint16(Config::TCPListenPort));
  m_ioService.post(
    std::bind(&Communicator::task_requestAnnounce, this, 0, "", nullptr));

  if(m_webserverInitiator) {
    m_ioService.post(
      std::bind(&webserver::Initiator::run, m_webserverInitiator.get()));
  }

  // The timer can only be enabled at this stage, after all other required data
  // structures have been initialised. Also, use a warmup time of 2 seconds -
  // the first 5 seconds do not need this, because other work has to be done.
  m_tickTimer.expires_from_now(std::chrono::seconds(2));
  m_tickTimer.async_wait(std::bind(&Communicator::tick, this));

  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service started.";
  m_ioService.run();
  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service ended.";
  m_runner->stop();
}

void
Communicator::exit()
{
  if(m_runner->m_running) {
    auto [nodeMap, lock] = m_clusterStatistics->getNodeMap();
    for(auto& it : nodeMap) {
      auto& node = it.second;
      task_offlineAnnouncement(node.getNetworkedNode());
    }
  }

  // Early stop m_runner, so that threads must not be notified more often if
  // this is called from a runner thread.
  m_runner->m_running = false;

  m_ioService.post([this]() {
    if(m_webserverInitiator) {
      m_webserverInitiator->stop();
      m_webserverInitiator.reset();
    }
    m_runner->stop();

    // Destruct all servers before io Service is stopped.
    m_udpServer.reset();
    m_tcpServer.reset();

    m_ioService.stop();
  });
}

void
Communicator::startRunner()
{
  if(!m_runner) {
    m_runner = std::make_shared<Runner>(this, m_config, m_log);
  }
  m_runner->start();
}

void
Communicator::signalHandler(const boost::system::error_code& error,
                            int signalNumber)
{
  if(signalNumber == SIGINT) {
    PARACUBER_LOG(m_logger, Trace) << "SIGINT detected.";
    exit();
  }
}

void
Communicator::checkAndTransmitClusterStatisticsChanges(bool force)
{
  if(m_clusterStatistics->clearChanged() || force) {
#ifdef ENABLE_INTERNAL_WEBSERVER
    // Transmit changes to API, so all web-clients can see the new cluster
    // statistics.
    if(m_webserverInitiator) {
      auto api = m_webserverInitiator->getAPI();
      if(api) {
        api->injectClusterStatisticsUpdate(*m_clusterStatistics);
      }
    }
#endif
  }
}

void
Communicator::listenForIncomingUDP(uint16_t port)
{
  using namespace boost::asio;
  try {
    m_udpServer = std::make_unique<UDPServer>(this, m_log, m_ioService, port);
  } catch(std::exception& e) {
    PARACUBER_LOG(m_logger, LocalError)
      << "Could not initialize server for incoming UDP connections on port "
      << port << "! Error: " << e.what();
  }
}

void
Communicator::listenForIncomingTCP(uint16_t port)
{
  using namespace boost::asio;
  try {
    m_tcpServer = std::make_unique<TCPServer>(this, m_log, m_ioService, port);
  } catch(std::exception& e) {
    PARACUBER_LOG(m_logger, LocalError)
      << "Could not initialize server for incoming TCP connections on port "
      << port << "! Error: " << e.what();
  }
}

void
Communicator::sendCNFToNode(std::shared_ptr<CNF> cnf,
                            CNFTree::Path path,
                            NetworkedNode* nn)
{
  if(path != 0) {
    // The root formula (path == 0) is always sent, other formulas on demand.
    cnf->getCNFTree().setRemote(path, nn->getId());
  }

  assert(nn);
  // This indirection is required to make this work from worker threads.
  m_ioService.post([this, cnf, path, nn]() {
    auto client = std::make_shared<TCPClient>(this,
                                              m_log,
                                              m_ioService,
                                              nn->getRemoteTcpEndpoint(),
                                              TCPClientMode::TransmitCNF,
                                              cnf,
                                              path);
    client->connect();
  });
  if(path == 0) {
    // Also send the allowance map to a CNF, if this transmits the root formula.
    sendAllowanceMapToNodeWhenReady(cnf, nn);
  }
}

void
Communicator::sendCNFResultToNode(std::shared_ptr<CNF> cnf,
                                  CNFTree::Path path,
                                  NetworkedNode* nn)
{
  assert(nn);
  // This indirection is required to make this work from worker threads.
  m_ioService.post([this, cnf, path, nn]() {
    auto client = std::make_shared<TCPClient>(this,
                                              m_log,
                                              m_ioService,
                                              nn->getRemoteTcpEndpoint(),
                                              TCPClientMode::TransmitCNFResult,
                                              cnf,
                                              path);
    client->connect();
  });
}

void
Communicator::sendAllowanceMapToNodeWhenReady(std::shared_ptr<CNF> cnf,
                                              NetworkedNode* nn)
{
  // First, wait for the allowance map to be ready.
  cnf->rootTaskReady.callWhenReady([this, cnf, nn](CaDiCaLTask& ptr) {
    // Registry is only initialised after the root task arrived.
    cnf->getCuberRegistry().allowanceMapWaiter.callWhenReady(
      [this, cnf, nn](cuber::Registry::AllowanceMap& map) {
        // This indirection is required to make this work from worker threads.
        m_ioService.post([this, cnf, nn]() {
          auto client =
            std::make_shared<TCPClient>(this,
                                        m_log,
                                        m_ioService,
                                        nn->getRemoteTcpEndpoint(),
                                        TCPClientMode::TransmitAllowanceMap,
                                        cnf);
          client->connect();
        });
      });
  });
}

void
Communicator::requestCNFPathInfo(CNFTree::Path p, int64_t handle, int64_t cnfId)
{
  std::shared_ptr<CNF> rootCNF;

  if(m_config->isDaemonMode()) {
    if(cnfId == 0) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Cannot request status on daemon node without ID!";
      return;
    }

    // TODO
    assert(false);
  } else {
    rootCNF = m_config->getClient()->getRootCNF();
  }

  if(!rootCNF) {
    PARACUBER_LOG(m_logger, LocalError)
      << "Cannot answer CNFTree path request without CNF instance!";
    return;
  }

  rootCNF->requestInfoGlobally(p, handle);
}

void
Communicator::injectCNFTreeNodeInfo(int64_t cnfId,
                                    int64_t handle,
                                    CNFTree::Path p,
                                    CNFTree::CubeVar v,
                                    CNFTree::StateEnum state,
                                    int64_t remote)
{
#ifdef ENABLE_INTERNAL_WEBSERVER
  webserver::API* api = m_webserverInitiator->getAPI();
  if(!api) {
    PARACUBER_LOG(m_logger, LocalWarning)
      << "Cannot inject CNFTreeNodeInfo into uninitialised webserver::API!";
    return;
  }
  api->injectCNFTreeNode(handle, p, v, state, remote);
#endif
}

void
Communicator::sendCNFTreeNodeStatusRequest(int64_t targetId,
                                           int64_t cnfId,
                                           CNFTree::Path p,
                                           int64_t handle)
{
  auto& statNode = m_clusterStatistics->getNode(targetId);
  auto* nn = statNode.getNetworkedNode();
  auto msgBuilder = m_udpServer->getMessageBuilder();
  auto req = msgBuilder.initCnfTreeNodeStatusRequest();
  req.setCnfId(cnfId);
  req.setPath(p);
  req.setHandle(handle);
  m_udpServer->sendBuiltMessage(nn->getRemoteUdpEndpoint(), false);
}

void
Communicator::tick()
{
  m_runner->checkTaskFactories();

  // Update workQueueSize
  auto& thisNode = m_clusterStatistics->getThisNode();
  thisNode.setWorkQueueSize(m_runner->getWorkQueueSize());
  {
    auto [v, lock] = m_runner->getTaskFactories();
    thisNode.applyTaskFactoryVector(v);
  }
  checkAndTransmitClusterStatisticsChanges();

  auto msg = m_udpServer->getMessageBuilder();
  auto nodeStatus = msg.initNodeStatus();
  nodeStatus.setWorkQueueSize(m_runner->getWorkQueueSize());

  if(m_config->isDaemonMode()) {
    auto daemon = m_config->getDaemon();
    assert(daemon);

    auto daemonStruct = nodeStatus.initDaemon();

    auto [contextMap, lock] = daemon->getContextMap();
    auto contextList = daemonStruct.initContexts(contextMap.size());

    auto contextStructIt = contextList.begin();

    for(auto& it : contextMap) {
      auto& context = *it.second;
      contextStructIt->setOriginator(context.getOriginatorId());
      contextStructIt->setState(static_cast<uint8_t>(context.getState()));
      contextStructIt->setFactorySize(context.getFactoryQueueSize());
      ++contextStructIt;
    }
  } else {
    auto client = m_config->getClient();
    assert(client);
  }

  // Send built message to all other known nodes.
  auto [nodeMap, lock] = m_clusterStatistics->getNodeMap();
  for(auto& it : nodeMap) {
    auto& node = it.second;
    if(node.getId() != m_config->getInt64(Config::Id)) {
      auto nn = node.getNetworkedNode();
      if(node.getFullyKnown() && nn) {
        m_udpServer->sendBuiltMessage(nn->getRemoteUdpEndpoint(), false, true);
      }
    }
  }

  m_clusterStatistics->rebalance();

  m_tickTimer.expires_from_now(
    std::chrono::milliseconds(m_config->getUint64(Config::TickMilliseconds)));
  m_tickTimer.async_wait(std::bind(&Communicator::tick, this));
}

void
setupNode(Node::Builder& n, Config& config)
{
  n.setName(std::string(config.getString(Config::LocalName)));
  n.setId(config.getInt64(Config::Id));
  n.setAvailableWorkers(config.getUint32(Config::ThreadCount));
  n.setWorkQueueCapacity(config.getUint64(Config::WorkQueueCapacity));
  n.setUdpListenPort(config.getUint16(Config::UDPListenPort));
  n.setTcpListenPort(config.getUint16(Config::TCPListenPort));
  n.setDaemonMode(config.isDaemonMode());
}

void
Communicator::task_requestAnnounce(int64_t id,
                                   std::string regex,
                                   NetworkedNode* nn)
{
  if(!m_udpServer)
    return;

  PARACUBER_LOG(m_logger, Trace) << "Send announcement request.";

  auto msg = m_udpServer->getMessageBuilder();

  auto req = msg.initAnnouncementRequest();
  auto nameMatch = req.initNameMatch();

  if(id != 0) {
    nameMatch.setId(id);
  } else if(regex != "") {
    nameMatch.setRegex(regex);
  }

  auto requester = req.initRequester();
  setupNode(requester, *m_config);

  if(nn) {
    m_udpServer->sendBuiltMessage(nn->getRemoteUdpEndpoint(), false);
  } else {
    m_udpServer->sendBuiltMessage(boost::asio::ip::udp::endpoint(
                                    boost::asio::ip::address_v4::broadcast(),
                                    m_config->getUint16(Config::UDPTargetPort)),
                                  false);
  }
}
void
Communicator::task_announce(NetworkedNode* nn)
{
  if(!m_udpServer)
    return;
  PARACUBER_LOG(m_logger, Trace) << "Send announcement.";

  auto msg = m_udpServer->getMessageBuilder();

  auto req = msg.initOnlineAnnouncement();

  auto node = req.initNode();
  setupNode(node, *m_config);

  if(nn) {
    m_udpServer->sendBuiltMessage(nn->getRemoteUdpEndpoint(), false);
  } else {
    m_udpServer->sendBuiltMessage(boost::asio::ip::udp::endpoint(
                                    boost::asio::ip::address_v4::broadcast(),
                                    m_config->getUint16(Config::UDPTargetPort)),
                                  false);
  }
}

void
Communicator::task_offlineAnnouncement(NetworkedNode* nn)
{
  if(!m_udpServer)
    return;
  PARACUBER_LOG(m_logger, Trace) << "Send offline announcement.";

  auto msg = m_udpServer->getMessageBuilder();

  auto off = msg.initOfflineAnnouncement();

  off.setReason("Shutdown");

  if(nn) {
    m_udpServer->sendBuiltMessage(nn->getRemoteUdpEndpoint(), false);
  } else {
    m_udpServer->sendBuiltMessage(boost::asio::ip::udp::endpoint(
                                    boost::asio::ip::address_v4::broadcast(),
                                    m_config->getUint16(Config::UDPTargetPort)),
                                  false,
                                  false);
  }
}

std::ostream&
operator<<(std::ostream& o, Communicator::TCPClientMode mode)
{
  switch(mode) {
    case Communicator::TCPClientMode::TransmitCNF:
      o << "Transmit CNF";
      break;
    case Communicator::TCPClientMode::TransmitAllowanceMap:
      o << "Transmit Allowance Map";
      break;
    case Communicator::TCPClientMode::TransmitCNFResult:
      o << "Transmit CNF Result";
      break;
  }
  return o;
}
}
