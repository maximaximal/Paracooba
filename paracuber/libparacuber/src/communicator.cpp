#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/runner.hpp"
#include <boost/asio.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::udp;

#define REC_BUF_SIZE 4096

namespace paracuber {
class Communicator::UDPServer
{
  public:
  UDPServer(LogPtr log, boost::asio::io_service& ioService, uint16_t port)
    : m_socket(ioService, udp::endpoint(udp::v4(), port))
    , m_logger(log->createLogger())
    , m_port(port)
  {
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
      boost::asio::buffer(m_recBuf),
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
      startReceive();
    }
  }

  private:
  udp::socket m_socket;
  udp::endpoint m_remoteEndpoint;
  boost::array<char, REC_BUF_SIZE> m_recBuf;
  Logger m_logger;
  uint16_t m_port;
};
class Communicator::TCPServer
{};

Communicator::Communicator(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_log(log)
  , m_ioService(std::make_shared<boost::asio::io_service>())
  , m_logger(log->createLogger())
  , m_signalSet(std::make_unique<boost::asio::signal_set>(*m_ioService, SIGINT))
{
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
  m_runner->stop();

  // Destruct all servers before io Service is stopped.
  m_udpServer.reset();
  m_tcpServer.reset();

  m_ioService->stop();
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
    m_udpServer = std::make_unique<UDPServer>(m_log, *m_ioService, port);
  } catch(std::exception& e) {
  }
}

void
Communicator::listenForIncomingTCP(uint16_t port)
{}

void
Communicator::task_firstContact()
{
  PARACUBER_LOG(m_logger, Trace) << "First contact ioService task started.";
}

}
