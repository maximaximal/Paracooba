#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/client.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/networked_node.hpp"
#include "../include/paracuber/runner.hpp"
#include <boost/asio.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/bind.hpp>
#include <cassert>
#include <mutex>
#include <regex>

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
}
void
applyMessageNodeStatusToStatsNode(const message::NodeStatus::Reader& src,
                                  ClusterStatistics::Node& tgt)
{
  tgt.setWorkQueueSize(src.getWorkQueueSize());
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
          PARACUBER_LOG(m_logger, Trace) << "  -> Online Announcement.";
          handleOnlineAnnouncement(msg);
          break;
        case Message::OFFLINE_ANNOUNCEMENT:
          PARACUBER_LOG(m_logger, Trace) << "  -> Offline Announcement.";
          handleOfflineAnnouncement(msg);
          break;
        case Message::ANNOUNCEMENT_REQUEST:
          PARACUBER_LOG(m_logger, Trace) << "  -> Announcement Request.";
          handleAnnouncementRequest(msg);
          break;
        case Message::NODE_STATUS:
          PARACUBER_LOG(m_logger, Trace) << "  -> Node Status.";
          handleNodeStatus(msg);
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
        std::make_unique<NetworkedNode>(m_remoteEndpoint);
      networkedNode->setUdpPort(node.getUdpListenPort());
      networkedNode->setTcpPort(node.getTcpListenPort());
      node.setNetworkedNode(std::move(networkedNode));
      nn = node.getNetworkedNode();
    }

    if(!node.getFullyKnown() &&
       node.getId() != m_communicator->m_config->getInt64(Config::Id)) {
      m_communicator->m_ioService->post(
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
      m_communicator->m_ioService->post(
        std::bind(&Communicator::task_announce,
                  m_communicator,
                  statisticsNode.getNetworkedNode()));

      m_communicator->sendCNFToNode(
        m_communicator->m_config->getClient()->getRootCNF(),
        statisticsNode.getNetworkedNode());
    }

    PARACUBER_LOG(m_logger, Info) << *m_clusterStatistics;
  }
  void handleOfflineAnnouncement(Message::Reader& msg)
  {
    const OfflineAnnouncement::Reader& reader = msg.getOfflineAnnouncement();

    m_clusterStatistics->removeNode(msg.getOrigin());

    PARACUBER_LOG(m_logger, Info) << *m_clusterStatistics;
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
      // A client does not need to send online announcements after the initial
      // announcement request. Only the root CNF needs to be sent, so the
      // context is initiated on the remote side, but this only if the remote is
      // a daemon that is accepting work.
      if(requester.getDaemonMode()) {
        m_communicator->sendCNFToNode(
          m_communicator->m_config->getClient()->getRootCNF(),
          statisticsNode.getNetworkedNode());
      }

      return;
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
      m_communicator->m_ioService->post(
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

    applyMessageNodeStatusToStatsNode(reader, statisticsNode);

    networkStatisticsNode(statisticsNode);
  }

  void handleSend(const boost::system::error_code& error,
                  std::size_t bytes,
                  std::mutex* sendBufMutex)
  {
    sendBufMutex->unlock();

    if(!error || error == boost::asio::error::message_size) {
    } else {
      PARACUBER_LOG(m_logger, LocalError)
        << "Error sending data from UDP socket. Error: " << error;
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
  void sendBuiltMessage(boost::asio::ip::udp::endpoint&& endpoint,
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
            std::shared_ptr<CNF> cnf)
    : m_comm(comm)
    , m_cnf(cnf)
    , m_endpoint(endpoint)
    , m_socket(ioService)
  {}
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

    PARACUBER_LOG(m_logger, Trace)
      << "Sending the CNF with previous=" << m_cnf->getPrevious() << " to "
      << m_endpoint << " started.";

    auto prev = m_cnf->getPrevious();

    int64_t id = 0;
    if(m_comm->m_config->isDaemonMode()) {
      id = m_cnf->getOriginId();
    } else {
      id = m_comm->m_config->getInt64(Config::Id);
    };

    uint32_t varCount = 0;
    if(m_comm->m_config->isDaemonMode()) {
      auto [context, inserted] =
        m_comm->m_config->getDaemon()->getOrCreateContext(nullptr, id);
      assert(!inserted);
      varCount = context.getCNFVarCount();
    } else {
      varCount = m_comm->m_config->getClient()->getCNFVarCount();
    }

    char buf[sizeof(prev) + sizeof(id) + sizeof(varCount)];
    *(reinterpret_cast<uint64_t*>(buf)) = prev;
    *(reinterpret_cast<uint32_t*>(buf + sizeof(prev))) = varCount;
    *(reinterpret_cast<int64_t*>(buf + sizeof(prev) + sizeof(varCount))) = id;

    m_socket.write_some(boost::asio::buffer(buf, sizeof(buf)));

    auto ptr = shared_from_this();

    m_socket.native_non_blocking(true);

    // Let the sending be handled by the CNF itself.
    m_cnf->send(&m_socket, [this, ptr]() {
      // Finished sending CNF!
      PARACUBER_LOG(m_logger, Trace)
        << "Sending the CNF with previous=" << m_cnf->getPrevious() << " to "
        << m_endpoint << " finished.";
    });
  }

  private:
  LoggerMT m_logger;
  Communicator* m_comm;
  std::shared_ptr<CNF> m_cnf;
  boost::asio::ip::tcp::socket m_socket;
  boost::asio::ip::tcp::endpoint m_endpoint;
  boost::array<char, REC_BUF_SIZE> m_recBuffer;
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
      ReceivingFile,
      ReceivingCube,
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
          case ReceivingFile:
            m_cnf->receive(nullptr, 0);
            break;
          case ReceivingCube:
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
        case ReceivingFile: {
          state_receivingFile(bytes);
          break;
        }
        case ReceivingCube: {
          state_receivingCube(bytes);
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
      uint64_t previous = *reinterpret_cast<uint64_t*>(&m_recBuffer[0]);
      m_pos += sizeof(previous);
      uint32_t varCount = *reinterpret_cast<uint32_t*>(&m_recBuffer[m_pos]);
      m_pos += sizeof(varCount);
      m_cnf = std::make_shared<CNF>(previous, varCount);

      int64_t originator = *reinterpret_cast<int64_t*>(&m_recBuffer[m_pos]);
      m_pos += sizeof(int64_t);

      if(m_comm->m_config->isDaemonMode()) {
        auto [context, inserted] =
          m_comm->m_config->getDaemon()->getOrCreateContext(
            m_cnf, originator, varCount);
        if(previous == 0 && inserted) {
          PARACUBER_LOG(m_logger, GlobalWarning)
            << "The same context should not have been created twice! "
               "Is a DIMACS file transmitted more than once?";
        }
        // The variable count may change if in the first transmission happens
        // before the CNF has been parsed by the client.
        context.setCNFVarCount(varCount);

      } else {
        // This is some other CNF that was sent back to the client. Handle it
        // using the main orchestrator.
      }

      // Handle remaining data of the stream.
      if(m_cnf->getPrevious() == 0) {
        state_receivingFile(bytes - m_pos);
        m_state = ReceivingFile;
      } else {
        m_state = ReceivingCube;
      }
    }
    void state_receivingFile(std::size_t bytes)
    {
      // The receiving file state exists to shuffle data from the network
      // stream into the locally created file. After finishing, the created
      // file can be given to a new task to be solved.

      if(bytes == 0) {
        // This can happen in the initial request.
        return;
      }

      m_cnf->receive(&m_recBuffer[m_pos], bytes);
    }
    void state_receivingCube(std::size_t bytes)
    {
      // This state is active if this is a subsequent cube transfer.
    }

    std::shared_ptr<CNF> m_cnf;

    boost::asio::io_service& m_ioService;
    boost::asio::ip::tcp::socket m_socket;
    boost::array<char, REC_BUF_SIZE> m_recBuffer;
    std::size_t m_pos = 0;
    Logger m_logger;
    Communicator* m_comm;
    State m_state = NewConnection;
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
  , m_ioService(std::make_shared<boost::asio::io_service>())
  , m_logger(log->createLogger())
  , m_signalSet(std::make_unique<boost::asio::signal_set>(*m_ioService, SIGINT))
  , m_clusterStatistics(std::make_shared<ClusterStatistics>(config, log))
{
  // Initialize communicator
  boost::asio::io_service::work work(*m_ioService);
  m_ioServiceWork = work;

  m_signalSet->async_wait(std::bind(&Communicator::signalHandler,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
}

Communicator::~Communicator()
{
  exit();
  PARACUBER_LOG(m_logger, Trace) << "Destruct Communicator.";
}

void
Communicator::run()
{
  using namespace boost::asio;

  if(!m_runner) {
    m_runner = std::make_shared<Runner>(this, m_config, m_log);
  }
  if(!m_runner->isRunning()) {
    m_runner->start();
  }

  listenForIncomingUDP(m_config->getUint16(Config::UDPListenPort));
  listenForIncomingTCP(m_config->getUint16(Config::TCPListenPort));
  m_ioService->post(
    std::bind(&Communicator::task_requestAnnounce, this, 0, "", nullptr));

  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service started.";
  m_ioService->run();
  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service ended.";
  m_runner->stop();
}

void
Communicator::exit()
{
  if(m_runner->m_running) {
    for(auto& it : m_clusterStatistics->getNodeMap()) {
      auto& node = it.second;
      task_offlineAnnouncement(node.getNetworkedNode());
    }
  }

  // Early stop m_runner, so that threads must not be notified more often if
  // this is called from a runner thread.
  m_runner->m_running = false;

  m_ioService->post([this]() {
    m_runner->stop();

    // Destruct all servers before io Service is stopped.
    m_udpServer.reset();
    m_tcpServer.reset();

    m_ioService->stop();
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
Communicator::listenForIncomingUDP(uint16_t port)
{
  using namespace boost::asio;
  try {
    m_udpServer = std::make_unique<UDPServer>(this, m_log, *m_ioService, port);
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
    m_tcpServer = std::make_unique<TCPServer>(this, m_log, *m_ioService, port);
  } catch(std::exception& e) {
    PARACUBER_LOG(m_logger, LocalError)
      << "Could not initialize server for incoming TCP connections on port "
      << port << "! Error: " << e.what();
  }
}

void
Communicator::sendCNFToNode(std::shared_ptr<CNF> cnf, NetworkedNode* nn)
{
  // This indirection is required to make this work from worker threads.
  m_ioService->post([this, cnf, nn]() {
    auto client = std::make_shared<TCPClient>(
      this, m_log, *m_ioService, nn->getRemoteTcpEndpoint(), cnf);
    client->connect();
  });
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
}
