#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/runner.hpp"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <mutex>

#include <capnp-schemas/message.capnp.h>
#include <capnp/serialize.h>

using boost::asio::ip::udp;

#define REC_BUF_SIZE 4096

namespace paracuber {
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
      PARACUBER_LOG(m_logger, Trace)
        << "Received " << bytes << " on UDP listener.";

      // Alignment handled by the creation of m_recBuf.

      ::capnp::FlatArrayMessageReader reader(
        kj::arrayPtr(reinterpret_cast<::capnp::word*>(m_recBuf.begin()),
                     bytes / sizeof(::capnp::word)));

      Message::Reader msg = reader.getRoot<Message>();
      switch(msg.which()) {
        case Message::ONLINE_ANNOUNCEMENT:
          PARACUBER_LOG(m_logger, Trace) << "  -> Online Announcement.";
          break;
        case Message::OFFLINE_ANNOUNCEMENT:
          PARACUBER_LOG(m_logger, Trace) << "  -> Offline Announcement.";
          break;
        case Message::ANNOUNCEMENT_REQUEST:
          PARACUBER_LOG(m_logger, Trace) << "  -> Announcement Request.";
          break;
        case Message::NODE_STATUS:
          PARACUBER_LOG(m_logger, Trace) << "  -> Node Status.";
          break;
      }

      startReceive();
    } else {
      PARACUBER_LOG(m_logger, LocalError)
        << "Error receiving data from UDP socket. Error: " << error;
    }
  }
  void handleSend(const boost::system::error_code& error,
                  std::size_t bytes,
                  std::mutex* sendBufMutex)
  {
    sendBufMutex->unlock();

    if(!error || error == boost::asio::error::message_size) {
      PARACUBER_LOG(m_logger, Trace) << "Sent " << bytes << " on UDP listener.";
    } else {
      PARACUBER_LOG(m_logger, LocalError)
        << "Error sending data from UDP socket. Error: " << error;
    }
  }

  inline ::capnp::MallocMessageBuilder& getMallocMessageBuilder()
  {
    return m_mallocMessageBuilder;
  }
  Message::Builder getMessageBuilder()
  {
    auto msg = getMallocMessageBuilder().getRoot<Message>();
    msg.setOrigin(m_communicator->getNodeId());
    msg.setId(m_communicator->getAndIncrementCurrentMessageId());
    return std::move(msg);
  }
  void sendBuiltMessage(boost::asio::ip::udp::endpoint&& endpoint)
  {
    // One copy cannot be avoided.
    auto buf = std::move(::capnp::messageToFlatArray(m_mallocMessageBuilder));

    {
      // Lock the mutex when sending and unlock it when the data has been sent
      // in the handleSend function. This way, async sending is still fast when
      // calculating stuff and does not block, but it DOES block when more
      // messages are sent.
      m_sendBufMutex.lock();

      m_sendBuf = std::move(buf);

      m_socket.async_send_to(
        boost::asio::buffer(reinterpret_cast<std::byte*>(&m_sendBuf[0]),
                            m_sendBuf.size() * sizeof(::capnp::word)),
        endpoint,
        boost::bind(&UDPServer::handleSend,
                    this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    &m_sendBufMutex));
    }
  }

  private:
  Communicator* m_communicator;
  Logger m_logger;
  uint16_t m_port;

  udp::socket m_socket;
  udp::endpoint m_remoteEndpoint;
  boost::array<::capnp::word, REC_BUF_SIZE> m_recBuf;

  static thread_local kj::Array<::capnp::word> m_sendBuf;
  static thread_local ::capnp::MallocMessageBuilder m_mallocMessageBuilder;
  static thread_local std::mutex m_sendBufMutex;
};

thread_local kj::Array<::capnp::word> Communicator::UDPServer::m_sendBuf;
thread_local std::mutex Communicator::UDPServer::m_sendBufMutex;
thread_local ::capnp::MallocMessageBuilder
  Communicator::UDPServer::m_mallocMessageBuilder;

class Communicator::TCPServer
{};

Communicator::Communicator(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_log(log)
  , m_ioService(std::make_shared<boost::asio::io_service>())
  , m_logger(log->createLogger())
  , m_signalSet(std::make_unique<boost::asio::signal_set>(*m_ioService, SIGINT))
{
  // Set current ID
  int16_t pid = static_cast<int16_t>(::getpid());
  int64_t uniqueNumber = config->getInt64(Config::Id);
  m_nodeId = ((int64_t)pid << 48) | uniqueNumber;

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

  // The first task to run is to make contact with the specified daemon solver.
  listenForIncomingUDP(m_config->getUint16(Config::UDPPort));
  m_ioService->post(std::bind(&Communicator::task_firstContact, this));

  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service started.";
  m_ioService->run();
  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service ended.";
  m_runner->stop();
}

void
Communicator::exit()
{
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
{}

void
Communicator::task_firstContact()
{
  PARACUBER_LOG(m_logger, Trace) << "First contact ioService task started.";

  auto msg = m_udpServer->getMessageBuilder();

  auto status = msg.initNodeStatus();

  PARACUBER_LOG(m_logger, Trace) << "Sending Data.";
  m_udpServer->sendBuiltMessage(boost::asio::ip::udp::endpoint(
    boost::asio::ip::address_v4::from_string("127.0.0.1"),
    m_config->getUint16(Config::UDPPort)));
}

}
