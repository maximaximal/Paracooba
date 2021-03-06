#ifndef PARACOOBA_NET_CONNECTION
#define PARACOOBA_NET_CONNECTION

#include <atomic>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>

#include <boost/asio/coroutine.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>

#include <boost/system/error_code.hpp>

#include "../daemon.hpp"
#include "../log.hpp"

#include "../messages/jobdescription_receiver.hpp"
#include "../messages/message_receiver.hpp"

namespace paracooba {
class CNF;
class ClusterNodeStore;

namespace messages {
class Message;
class JobDescription;
}

namespace net {
/** @brief Class representing a connection between two compute nodes.
 *
 * This class handles the complete connection live-cycle. Connections are
 * established over TCP sockets. If a socket is ended, it is tried to reconnect
 * to the remote host. Amount of retries can be tuned with
 * Config::ConnectionRetries.
 */
class Connection
{
  public:
  enum Mode
  {
    TransmitCNF,
    TransmitJobDescription,
    TransmitControlMessage,
    TransmitEndToken,
    TransmitModeUnknown
  };

  enum ResumeMode
  {
    EndAfterShutdown,
    RestartAfterShutdown
  };

  struct EndTokenTag
  {};

  using SendItem = std::variant<std::shared_ptr<CNF>,
                                messages::Message,
                                messages::JobDescription,
                                EndTokenTag>;
  using ContextPtr = Daemon::Context*;

  struct SendQueueEntry
  {
    SendQueueEntry();
    SendQueueEntry(const SendQueueEntry& o);
    SendQueueEntry(std::shared_ptr<CNF> cnf,
                   const SuccessCB& sendFinishedCB = EmptySuccessCB);
    SendQueueEntry(const messages::Message& msg,
                   const SuccessCB& sendFinishedCB = EmptySuccessCB);
    SendQueueEntry(const messages::JobDescription& jd,
                   const SuccessCB& sendFinishedCB = EmptySuccessCB);
    SendQueueEntry(EndTokenTag endTokenTag,
                   const SuccessCB& sendFinishedCB = EmptySuccessCB);
    ~SendQueueEntry();

    std::unique_ptr<SendItem> sendItem;
    SuccessCB sendFinishedCB;
  };

  struct State
  {
    explicit State(boost::asio::io_service& ioService,
                   LogPtr log,
                   ConfigPtr config,
                   ClusterNodeStore& clusterNodeStore,
                   messages::MessageReceiver& msgRec,
                   messages::JobDescriptionReceiverProvider& jdRecProv,
                   uint16_t retries);
    ~State();

    boost::asio::io_service& ioService;
    boost::asio::ip::tcp::socket socket;
    LogPtr log;
    Logger logger;
    ConfigPtr config;
    ClusterNodeStore& clusterNodeStore;
    messages::MessageReceiver& messageReceiver;
    messages::JobDescriptionReceiverProvider& jobDescriptionReceiverProvider;
    NetworkedNodeWeakPtr remoteNN;
    std::shared_ptr<CNF> cnf;
    std::string remote;

    boost::asio::streambuf recvStreambuf = boost::asio::streambuf();
    boost::asio::streambuf sendStreambuf = boost::asio::streambuf();
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::steady_timer steadyTimer;
    ContextPtr context = nullptr;
    int64_t remoteId = 0;
    int64_t thisId = 0;
    uint64_t size = 0;
    uint64_t sendSize = 0;
    bool exit = false;

    Mode currentMode = TransmitModeUnknown;
    Mode sendMode = TransmitModeUnknown;
    ResumeMode resumeMode = EndAfterShutdown;

    std::mutex sendQueueMutex;
    std::queue<SendQueueEntry> sendQueue;
    std::unique_ptr<SendItem> currentSendItem;
    SuccessCB currentSendFinishedCB = EmptySuccessCB;

    boost::asio::coroutine readCoro;
    boost::asio::coroutine writeCoro;

    std::atomic_bool connectionEstablished = false;
    std::atomic_bool connectionEnding = false;
    std::atomic_bool currentlySending = true;
    std::atomic_uint16_t connectionTry = 0;
  };

  explicit Connection(
    boost::asio::io_service& ioService,
    LogPtr log,
    ConfigPtr config,
    ClusterNodeStore& clusterNodeStore,
    messages::MessageReceiver& msgReceiver,
    messages::JobDescriptionReceiverProvider& jdReceiverProvider,
    uint16_t retries = 0);

  Connection(const Connection& conn);

  virtual ~Connection();

  void sendCNF(std::shared_ptr<CNF> cnf,
               const SuccessCB& sendFinishedCB = EmptySuccessCB);
  void sendMessage(const messages::Message& msg,
                   const SuccessCB& sendFinishedCB = EmptySuccessCB);
  void sendJobDescription(const messages::JobDescription& jd,
                          const SuccessCB& sendFinishedCB = EmptySuccessCB);
  void sendEndToken();
  void sendSendQueueEntry(SendQueueEntry&& e);

  void readHandler(boost::system::error_code ec = boost::system::error_code(),
                   size_t n = 0);

  boost::asio::ip::tcp::socket& socket() { return m_state->socket; }
  const boost::asio::ip::tcp::socket& socket() const { return m_state->socket; }

  void connect(const NetworkedNodePtr& nn);
  void connect(const std::string& remote);

  bool isConnectionEstablished() const
  {
    return m_state->connectionEstablished;
  }
  bool isConnectionEnding() const { return m_state->connectionEnding; }
  uint16_t getConnectionTries() const { return m_state->connectionTry; }
  int64_t getRemoteId() const { return m_state->remoteId; }

  Mode getRecvMode() const { return m_state->currentMode; }
  Mode getSendMode() const { return m_state->sendMode; }

  void exit() { isExit() = true; }

  void resetRemoteNN() { remoteNN().reset(); }

  boost::asio::ip::tcp::endpoint getRemoteTcpEndpoint() const;

  private:
  void writeHandler(boost::system::error_code ec = boost::system::error_code(),
                    size_t n = 0);

  boost::asio::io_service& ioService() { return m_state->ioService; }
  boost::asio::streambuf& recvStreambuf() { return m_state->recvStreambuf; }
  boost::asio::streambuf& sendStreambuf() { return m_state->sendStreambuf; }
  boost::asio::ip::tcp::resolver& resolver() { return m_state->resolver; }
  boost::asio::steady_timer& steadyTimer() { return m_state->steadyTimer; }
  Logger& logger();
  int64_t& thisId() { return m_state->thisId; }
  int64_t& remoteId() { return m_state->remoteId; }
  uint64_t& size() { return m_state->size; }
  uint64_t& sendSize() { return m_state->sendSize; }
  uint8_t& currentModeAsUint8()
  {
    return reinterpret_cast<uint8_t&>(m_state->currentMode);
  }
  Mode& currentMode() { return m_state->currentMode; }
  uint8_t& sendModeAsUint8()
  {
    return reinterpret_cast<uint8_t&>(m_state->sendMode);
  }
  Mode& sendMode() { return m_state->sendMode; }

  ResumeMode& resumeMode() { return m_state->resumeMode; }
  std::mutex& sendQueueMutex() { return m_state->sendQueueMutex; }
  std::queue<SendQueueEntry>& sendQueue() { return m_state->sendQueue; }
  std::unique_ptr<SendItem>& currentSendItem()
  {
    return m_state->currentSendItem;
  }
  SuccessCB& currentSendFinishedCB() { return m_state->currentSendFinishedCB; }
  ConfigPtr& config() { return m_state->config; }
  ClusterNodeStore& clusterNodeStore() { return m_state->clusterNodeStore; }
  ContextPtr& context() { return m_state->context; }
  std::shared_ptr<CNF>& cnf() { return m_state->cnf; }
  NetworkedNodeWeakPtr& remoteNN() { return m_state->remoteNN; }
  messages::MessageReceiver& messageReceiver()
  {
    return m_state->messageReceiver;
  }
  messages::JobDescriptionReceiverProvider& jobDescriptionReceiverProvider()
  {
    return m_state->jobDescriptionReceiverProvider;
  }

  std::atomic_bool& connectionEstablished()
  {
    return m_state->connectionEstablished;
  }
  std::atomic_bool& connectionEnding() { return m_state->connectionEnding; }
  std::atomic_bool& currentlySending() { return m_state->currentlySending; }
  std::atomic_uint16_t& connectionTry() { return m_state->connectionTry; }
  std::string& remote() { return m_state->remote; }

  boost::asio::coroutine& readCoro() { return m_state->readCoro; }
  boost::asio::coroutine& writeCoro() { return m_state->writeCoro; }

  static Mode getSendModeFromSendItem(const SendItem& sendItem);

  std::shared_ptr<State> m_state;

  bool getLocalCNF();
  void receiveIntoCNF(size_t bytes_received);
  void receiveSerializedMessage(size_t bytes_received);
  bool& isExit() { return m_state->exit; }
  void sendNodeStatus();

  void popNextSendItem();
  bool initRemoteNN();
  void reconnect();

  void reconnectAfterMS(uint32_t milliseconds);

  void enrichLogger();
};

constexpr const char*
ConnectionModeToStr(Connection::Mode mode)
{
  switch(mode) {
    case Connection::Mode::TransmitCNF:
      return "TransmitCNF";
    case Connection::Mode::TransmitJobDescription:
      return "TransmitJobDescription";
    case Connection::Mode::TransmitControlMessage:
      return "TransmitControlMessage";
    case Connection::Mode::TransmitEndToken:
      return "TransmitEndToken";
    case Connection::Mode::TransmitModeUnknown:
      return "TransmitModeUnknown";
  }
  return "Unknown Mode";
}

constexpr const char*
ConnectionResumeModeToStr(Connection::ResumeMode resumeMode)
{
  switch(resumeMode) {
    case Connection::ResumeMode::EndAfterShutdown:
      return "EndAfterShutdown";
    case Connection::ResumeMode::RestartAfterShutdown:
      return "RestartAfterShutdown";
  }
  return "Unknown Resume Mode";
}

std::ostream&
operator<<(std::ostream& o, Connection::Mode mode);

std::ostream&
operator<<(std::ostream& o, Connection::ResumeMode resumeMode);
}
}

#endif
