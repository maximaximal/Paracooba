#include "tcp_connection.hpp"
#include "file_sender.hpp"
#include "packet.hpp"
#include "paracooba/broker/broker.h"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/status.h"
#include "paracooba/communicator/communicator.h"
#include "service.hpp"
#include "tcp_connection_initiator.hpp"

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <thread>
#if BOOST_VERSION >= 106600
#include <boost/asio/io_context.hpp>
#else
#include <boost/asio/io_service.hpp>
#endif
#include <boost/asio/placeholders.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstdarg>
#include <functional>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>
#include <fstream>

#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/file.h>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/module.h>

#ifdef ENABLE_DISTRAC
#include <distrac_paracooba.h>
#endif

#include <sstream>

#define REC_BUF_SIZE 4096u
#define MAX_BUF_SIZE 10000000u

namespace parac::communicator {
struct TCPConnection::EndTag {};
struct TCPConnection::ACKTag {};

struct TCPConnection::InitiatorMessage {
  parac_id sender_id;
  uint16_t tcp_port;
};

struct TCPConnection::SendQueueEntry {
  SendQueueEntry(const SendQueueEntry& e) = default;
  SendQueueEntry(SendQueueEntry&& e) = default;

  SendQueueEntry(parac_message& msg) noexcept
    : value(msg)
    , transmitMode(TransmitMessage) {
    header.kind = msg.kind;
    header.size = msg.length;
    header.originator = msg.originator_id;
  }
  SendQueueEntry(parac_file& file) noexcept
    : value(file)
    , transmitMode(TransmitFile) {
    header.kind = PARAC_MESSAGE_FILE;
    header.size = FileSender::file_size(file.path);
    header.originator = file.originator;
  }
  SendQueueEntry(EndTag& end) noexcept
    : value(end)
    , transmitMode(TransmitEnd) {
    header.kind = PARAC_MESSAGE_END;
    header.size = 0;
  }
  SendQueueEntry(parac_message&& msg) noexcept
    : value(std::move(msg))
    , transmitMode(TransmitMessage) {
    header.kind = msg.kind;
    header.size = msg.length;
    header.originator = msg.originator_id;
  }
  SendQueueEntry(parac_file&& file) noexcept
    : value(std::move(file))
    , transmitMode(TransmitFile) {
    header.kind = PARAC_MESSAGE_FILE;
    header.size = FileSender::file_size(file.path);
    header.originator = file.originator;
  }
  SendQueueEntry(EndTag&& end) noexcept
    : value(std::move(end))
    , transmitMode(TransmitEnd) {
    header.kind = PARAC_MESSAGE_END;
    header.size = 0;
  }

  SendQueueEntry(uint32_t id, parac_status status)
    : transmitMode(TransmitACK) {
    header.kind = PARAC_MESSAGE_ACK;
    header.ack_status = status;
    header.number = id;
    header.size = 0;
  }

  using value_type =
    std::variant<parac_message_wrapper, parac_file_wrapper, EndTag, ACKTag>;

  PacketHeader header;
  value_type value;
  TransmitMode transmitMode;
  std::chrono::time_point<std::chrono::steady_clock> queued =
    std::chrono::steady_clock::now();
  std::chrono::time_point<std::chrono::steady_clock> sent;

  parac_message_wrapper& message() {
    return std::get<parac_message_wrapper>(value);
  }
  parac_file_wrapper& file() { return std::get<parac_file_wrapper>(value); }

  void operator()(parac_status status) {
    switch(transmitMode) {
      case TransmitMessage:
        message().doCB(status);
        break;
      case TransmitFile:
        file().doCB(status);
        break;
      default:
        break;
    }
  }

  ~SendQueueEntry() {}
};

using TCPConnectionSendQueue = std::queue<TCPConnection::SendQueueEntry>;
using TCPConnectionSentMap =
  std::map<decltype(PacketHeader::number), TCPConnection::SendQueueEntry>;

struct TCPConnectionPayload {
  TCPConnectionPayload(TCPConnectionSendQueue&& q, TCPConnectionSentMap&& m)
    : queue(std::move(q))
    , map(std::move(m)) {}

  TCPConnectionSendQueue queue;
  TCPConnectionSentMap map;
};

void
TCPConnectionPayloadDestruct(TCPConnectionPayload* payload) {
  delete payload;
}

struct TCPConnection::State {
  explicit State(Service& service,
                 std::unique_ptr<boost::asio::ip::tcp::socket> socket,
                 int connectionTry)
    : service(service)
    , socket(std::move(socket))
    , connectionTry(connectionTry)
    , writeInitiatorMessage(
        { service.handle().id, service.currentTCPListenPort() })
    , readTimer(service.ioContext())
    , keepaliveTimer(service.ioContext()) {
    if(connectionTry < 0) {
      resumeMode = EndAfterShutdown;
    }
  }
  explicit State(Service& service,
                 std::unique_ptr<boost::asio::ip::tcp::socket> socket,
                 int connectionTry,
                 TCPConnectionPayloadPtr ptr)
    : service(service)
    , socket(std::move(socket))
    , connectionTry(connectionTry)
    , writeInitiatorMessage(
        { service.handle().id, service.currentTCPListenPort() })
    , sendQueue(std::move(ptr->queue))
    , sentBuffer(std::move(ptr->map))
    , readTimer(service.ioContext())
    , keepaliveTimer(service.ioContext()) {
    service.addOutgoingMessageToCounter(sendQueue.size());
  }

  ~State() {
    if(!service.stopped()) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "Connection to remote {} ended with resume mode {}.",
                remoteId(),
                resumeMode);
    }

    if(compute_node) {
      compute_node->communicator_free(compute_node);
      compute_node->connection_string = nullptr;
      compute_node = nullptr;
    }
    service.removeOutgoingMessageFromCounter(sendQueue.size());
    if(!service.stopped()) {
      switch(resumeMode) {
        case RestartAfterShutdown: {
          if(connectionTry >= 0 &&
             connectionTry < service.connectionRetries()) {
            // Reconnects only happen if this side of the connection is the
            // original initiating side. The other side will end the connection.

            parac_log(PARAC_COMMUNICATOR,
                      PARAC_TRACE,
                      "Reconnect to endpoint {} (remote id {}), try {}.",
                      remote_endpoint(),
                      remoteId(),
                      connectionTry);
            TCPConnectionInitiator initiator(
              service, cachedRemoteEndpoint, nullptr, ++connectionTry);

            initiator.setTCPConnectionPayload(generatePayload());
          } else if(connectionTry == -1 && remoteId() != 0) {
            service.registerTCPConnectionPayload(remoteId(), generatePayload());
          }
          break;
        }
        case EndAfterShutdown:
          // Notify all items still in queue that the connection has been
          // closed and transmission was unsuccessful.
          while(!sendQueue.empty()) {
            sendQueue.front()(PARAC_CONNECTION_CLOSED);
            sendQueue.pop();
          }
          for(auto& e : sentBuffer) {
            e.second(PARAC_CONNECTION_CLOSED);
          }
          break;
      }
    }

    /*
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "TCPConnection::State to {} destruct, connection dropping. "
              "Position in readHandler {}, writeHandler {}.",
              remoteId(),
              *reinterpret_cast<int*>(
                &readCoro),// Hacky reading for debugging purposes.
              *reinterpret_cast<int*>(&writeCoro));
              */
  }

  Service& service;
  std::unique_ptr<boost::asio::ip::tcp::socket> socket;

  std::vector<char> recvBuf;
  std::atomic_int connectionTry = 0;

  std::atomic<TransmitMode> transmitMode = TransmitInit;
  ResumeMode resumeMode = EndAfterShutdown;

  boost::asio::coroutine writeCoro;
  std::mutex sendQueueMutex;
  InitiatorMessage writeInitiatorMessage;

  boost::asio::coroutine readCoro;
  PacketHeader readHeader;
  PacketFileHeader readFileHeader;
  InitiatorMessage readInitiatorMessage;
  uint32_t sendMessageNumber = 0;

  struct FileOstream {
    std::ofstream o;
    std::string path;
    std::string originalFilename;
  };
  std::unique_ptr<FileOstream> readFileOstream;

  parac_compute_node* compute_node = nullptr;
  parac_id remoteIdCache = 0;
  std::string connectionString;

  parac_id remoteId() const { return remoteIdCache; }
  std::string remote_endpoint() {
    if(socket->is_open()) {
      std::stringstream s;
      s << socket->remote_endpoint();
      return s.str();
    } else {
      return "(socket closed)";
    }
  }

  TCPConnectionPayloadPtr generatePayload() {
    return TCPConnectionPayloadPtr(
      new TCPConnectionPayload(std::move(sendQueue), std::move(sentBuffer)),
      &TCPConnectionPayloadDestruct);
  }

  Lifecycle lifecycle = Initializing;
  std::atomic_flag currentlySending = true;
  boost::asio::ip::tcp::endpoint cachedRemoteEndpoint;

  std::queue<SendQueueEntry> sendQueue;
  using SentBuffer = std::map<decltype(PacketHeader::number), SendQueueEntry>;
  SentBuffer sentBuffer;

  boost::asio::steady_timer readTimer;
  boost::asio::steady_timer keepaliveTimer;
};

template<typename S, typename B, typename CB>
void
TCPConnection::async_write(S& socket, B&& buffer, CB& cb) {
  boost::asio::async_write(socket, buffer, cb);
}

template<typename S, typename B, typename CB>
void
TCPConnection::async_read(S& socket, B&& buffer, CB cb) {
  auto timeout_handler = [cb](const boost::system::error_code& ec) {
    if(!ec) {
      // The read timed out! Call the CB with a timeout error.
      parac_log(PARAC_COMMUNICATOR,
                PARAC_GLOBALWARNING,
                "Connection run into timeout!");

      std::function<void(boost::system::error_code, size_t)> timeout_func = cb;
      timeout_func(boost::system::error_code(boost::system::errc::timed_out,
                                             boost::system::generic_category()),
                   0);
    }
  };

  auto s = state();
  if(!s)
    return;

  s->readTimer.expires_from_now(
    std::chrono::milliseconds(s->service.networkTimeoutMS()));
  s->readTimer.async_wait(timeout_handler);

  auto read_handler = [cb, *this](const boost::system::error_code& ec,
                                  size_t bytes_received) {
    auto s = state();
    if(!s)
      return;
    s->readTimer.cancel();
    std::function<void(boost::system::error_code, size_t)> cb_func = cb;
    cb_func(ec, bytes_received);
  };

  boost::asio::async_read(socket, buffer, read_handler);
}

TCPConnection::TCPConnection(
  Service& service,
  std::unique_ptr<boost::asio::ip::tcp::socket> socket,
  int connectionTry,
  TCPConnectionPayloadPtr ptr) {
  if(ptr) {
    m_stateStore = std::make_shared<State>(
      service, std::move(socket), connectionTry, std::move(ptr));
  } else {
    m_stateStore =
      std::make_shared<State>(service, std::move(socket), connectionTry);
  }

  // Set at beginning, only unset if write_handler yields. Set again at
  // send(SendQueueItem&).
  state()->currentlySending.test_and_set();

  auto weakConn = TCPConnection(*this, WeakStatePtr(state()));
  weakConn.writeHandler(boost::system::error_code(), 0);

  readHandler(boost::system::error_code(), 0);
}

TCPConnection::~TCPConnection() {

  if(auto s = state()) {

    /*
    parac_log(
      PARAC_COMMUNICATOR,
      PARAC_LOCALERROR,
      "TCPConnection to {} destruct. State use count is {}. Position in "
      "readHandler {}, writeHandler {}.",
      s->remoteId(),
      s.use_count(),
      *reinterpret_cast<int*>(
        &s->readCoro),// Hacky reading for debugging purposes.
      *reinterpret_cast<int*>(&s->writeCoro));
      */

    if(!s->service.stopped()) {
      if(s->transmitMode != TransmitEnd && s.use_count() == 1) {
        // This is the last connection, so this is also the last one having a
        // reference to the state. This is a clean shutdown of a connection, the
        // socket will be ended. To differentiate between this clean shutdown
        // and a dirty one, an EndToken must be transmitted. This is the last
        // action.
        TCPConnection(*this).send(EndTag{});
      }
    }
  }
}

void
TCPConnection::send(parac_message&& message) {
  send(SendQueueEntry(std::move(message)));
}
void
TCPConnection::send(parac_file&& file) {
  send(SendQueueEntry(std::move(file)));
}
void
TCPConnection::send(EndTag&& end) {
  send(SendQueueEntry(std::move(end)));
}
void
TCPConnection::send(parac_message& message) {
  send(SendQueueEntry(message));
}
void
TCPConnection::send(parac_file& file) {
  send(SendQueueEntry(file));
}
void
TCPConnection::send(EndTag& end) {
  send(SendQueueEntry(end));
}
void
TCPConnection::sendACK(uint32_t id, parac_status status) {
  send(SendQueueEntry(id, status));
}

void
TCPConnection::send(SendQueueEntry&& e) {
  auto s = state();
  if(!s)
    return;
  {
    std::lock_guard lock(s->sendQueueMutex);
    if(e.header.kind != PARAC_MESSAGE_ACK) {
      e.header.number = s->sendMessageNumber++;
    }

    if(e.header.kind != PARAC_MESSAGE_KEEPALIVE) {
      s->service.addOutgoingMessageToCounter();
    } else {
      assert(s->sendQueue.size() == 0);
    }

    s->sendQueue.emplace(std::move(e));
  }
  if(!s->currentlySending.test_and_set()) {
    if(s->service.ioContextThreadId() == std::this_thread::get_id()) {
      writeHandler(boost::system::error_code(), 0);
    } else {
      // The Socket is not thread-safe, so when sending from some other thread,
      // the send must be issued over the ioContext thread by posting work onto
      // the context.
      auto weakConn = TCPConnection(*this, WeakStatePtr(state()));
      s->service.ioContext().post([weakConn]() mutable {
        weakConn.writeHandler(boost::system::error_code(), 0);
      });
    }
  }
}

bool
TCPConnection::shouldHandlerBeEnded() {
  auto s = state();
  if(!s)
    return true;

  if(s->compute_node) {
    auto st = s->compute_node->state;
    if(st == PARAC_COMPUTE_NODE_TIMEOUT || st == PARAC_COMPUTE_NODE_EXITED) {
      return true;
    }
  }
  if(s->service.stopped()) {
    return true;
  }
  return false;
}

bool
TCPConnection::handleInitiatorMessage(const InitiatorMessage& init) {
  auto s = state();
  assert(s);

  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Handle initiator message on node {} from node {}.",
            s->service.handle().id,
            init.sender_id);

  if(init.sender_id == s->service.handle().id) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_DEBUG,
              "Not accepting connection from same node ({}).",
              init.sender_id);
    return false;
  }

  auto compute_node_store =
    s->service.handle().modules[PARAC_MOD_BROKER]->broker->compute_node_store;

  TCPConnection* conn = new TCPConnection(*this, WeakStatePtr(state()));
  assert(conn->m_stateStore.index() == 0);

  s->compute_node = compute_node_store->get_with_connection(
    compute_node_store,
    init.sender_id,
    &TCPConnection::compute_node_free_func,
    conn,
    &TCPConnection::compute_node_send_message_to_func,
    &TCPConnection::compute_node_send_file_to_func);

  if(!s->compute_node) {
    delete conn;
    return false;
  }

  s->remoteIdCache = s->compute_node->id;

  {
    std::stringstream connStr;
    auto a = s->socket->remote_endpoint().address();
    connStr << (a.is_v6() ? "[" : "") << a.to_string() << (a.is_v6() ? "]" : "")
            << ':' << init.tcp_port;
    s->connectionString = connStr.str();
    s->compute_node->connection_string = s->connectionString.c_str();
    s->cachedRemoteEndpoint = s->socket->remote_endpoint();
  }

  return true;
}

bool
TCPConnection::handleReceivedACK(const PacketHeader& ack) {
  auto s = state();
  assert(s);

  State::SentBuffer::iterator it;
  {
    std::unique_lock lock(s->sendQueueMutex);
    it = s->sentBuffer.find(ack.number);
    if(it == s->sentBuffer.end()) {
      return false;
    }
  }

  auto& sentItem = it->second;
  sentItem(ack.ack_status);
  sentItem(PARAC_TO_BE_DELETED);

#ifdef ENABLE_DISTRAC
  auto distrac = s->service.handle().distrac;
  if(distrac) {
    parac_ev_send_msg_ack e{ s->remoteId(),
                             sentItem.header.kind,
                             sentItem.header.number };
    distrac_push(distrac, &e, PARAC_EV_SEND_MSG_ACK);
  }
#endif

  {
    std::unique_lock lock(s->sendQueueMutex);
    s->sentBuffer.erase(it);
  }
  return true;
}

bool
TCPConnection::handleReceivedMessage() {
  auto s = state();
  assert(s);

  parac_message msg;
  struct data {
    parac_status status = PARAC_UNDEFINED;
    bool returned = false;
  };
  data d;

  msg.kind = s->readHeader.kind;
  msg.length = s->readHeader.size;
  msg.data = static_cast<char*>(s->recvBuf.data());
  msg.data_to_be_freed = false;
  msg.userdata = &d;
  msg.originator_id = s->readHeader.originator;
  msg.origin = s->compute_node;
  msg.cb = [](parac_message* msg, parac_status status) {
    data* d = static_cast<data*>(msg->userdata);
    d->status = status;
    d->returned = true;
  };
  assert(msg.data);

  assert(s->compute_node->receive_message_from);
  s->compute_node->receive_message_from(s->compute_node, &msg);

  // Callback must be called immediately! This gives the status that is
  // then passed back.
  assert(d.returned);

  if(d.status == PARAC_ABORT_CONNECTION) {
    return false;
  }

  sendACK(s->readHeader.number, d.status);
  return true;
}

void
TCPConnection::handleReceivedFileStart() {
  auto s = state();
  assert(s);

  std::string name = s->readFileHeader.name;

  boost::filesystem::path p(s->service.temporaryDirectory());

  if(s->readHeader.originator != s->service.handle().id) {
    p /= std::to_string(s->readHeader.originator);
    boost::filesystem::create_directory(p);
  }
  p /= name;

  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Start receiving file \"{}\" from {} to {}",
            name,
            s->remoteId(),
            p);

  s->readFileOstream = std::make_unique<State::FileOstream>(
    State::FileOstream{ std::ofstream{ p.string().c_str(), std::ios::binary },
                        p.string(),
                        s->readFileHeader.name });

  assert(s->readFileOstream->o);
  assert(s->readFileOstream->o.is_open());
}

void
TCPConnection::handleReceivedFileChunk() {
  auto s = state();
  assert(s);

  auto& o = s->readFileOstream;
  assert(o);
  o->o.write(s->recvBuf.data(), s->recvBuf.size());
}

void
TCPConnection::handleReceivedFile() {
  auto s = state();
  assert(s);

  parac_file file;
  struct data {
    parac_status status;
    bool returned = false;
  };
  data d;

  s->readFileOstream->o.flush();

  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Finished receiving file to \"{}\" from {} with originator {}. "
            "Wrote {} bytes.",
            s->readFileOstream->path,
            s->remoteId(),
            s->readHeader.originator,
            s->readFileOstream->o.tellp());

  file.cb = [](parac_file* file, parac_status status) {
    data* d = static_cast<data*>(file->userdata);
    d->status = status;
    d->returned = true;
  };
  file.userdata = &d;
  file.path = s->readFileOstream->path.c_str();
  file.originator = s->readHeader.originator;

  assert(s->compute_node);
  assert(s->compute_node->receive_file_from);

  s->compute_node->receive_file_from(s->compute_node, &file);

  // Callback must be called immediately! This gives the status that is
  // then passed back.
  assert(d.returned);

  sendACK(s->readHeader.number, d.status);
}

void
TCPConnection::compute_node_free_func(parac_compute_node* n) {
  assert(n);
  if(n->communicator_userdata) {
    TCPConnection* conn = static_cast<TCPConnection*>(n->communicator_userdata);
    auto s = conn->state();
    if(s) {
      if(s->compute_node == n) {
        s->compute_node = nullptr;
      }
      conn->send(EndTag());
    }
    n->connection_dropped(n);
    n->communicator_userdata = nullptr;
    delete conn;
  }
}

void
TCPConnection::compute_node_send_message_to_func(parac_compute_node* n,
                                                 parac_message* msg) {
  assert(n);
  assert(msg);
  if(!n->communicator_userdata) {
    msg->cb(msg, PARAC_CONNECTION_CLOSED);
    return;
  }
  TCPConnection* conn = static_cast<TCPConnection*>(n->communicator_userdata);
  auto s = conn->state();
  if(s) {
    conn->send(*msg);
  }
}

void
TCPConnection::compute_node_send_file_to_func(parac_compute_node* n,
                                              parac_file* file) {
  assert(n);
  assert(n->communicator_userdata);
  TCPConnection* conn = static_cast<TCPConnection*>(n->communicator_userdata);
  auto s = conn->state();
  if(s) {
    conn->send(*file);
  }
}

#define BUF(SOURCE) boost::asio::buffer(&SOURCE, sizeof(SOURCE))
#define VBUF(SOURCE) boost::asio::buffer(SOURCE)

#include <boost/asio/yield.hpp>
void
TCPConnection::readHandler(boost::system::error_code ec,
                           size_t bytes_received) {
  using namespace boost::asio;
  auto rh = std::bind(&TCPConnection::readHandler,
                      *this,
                      std::placeholders::_1,
                      std::placeholders::_2);

  auto s = state();
  assert(s);

  if(shouldHandlerBeEnded()) {
    s->socket->close();
    return;
  }

  if(ec) {
    boost::system::error_code ec;
    s->socket->cancel(ec);
    s->socket->close(ec);
  }

  if(ec == boost::asio::error::eof) {
    // End of file detected. This means the connection should shut down
    // cleanly, without bloating the exception handling in the internal loop.
    // Eventual reconnects are handled by destructors.
    return;
  }

  if(s->compute_node) {
    s->compute_node->bytes_received += bytes_received;
  }

  reenter(&s->readCoro) {
    yield async_read(*s->socket, BUF(s->readInitiatorMessage), rh);

    if(ec) {
      parac_log(
        PARAC_COMMUNICATOR,
        PARAC_LOCALERROR,
        "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
        "reading initiator message: {}",
        s->remote_endpoint(),
        s->remoteId(),
        ec);
      return;
    }

    if(!handleInitiatorMessage(s->readInitiatorMessage)) {
      // This only means that the connection was already created.
      return;
    }

    while(true) {
      if(ec) {
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_LOCALERROR,
          "Error in TCPConnection to endpoint {} (ID {}) readHandler before "
          "reading: {}",
          s->remote_endpoint(),
          s->remoteId(),
          ec);
        return;
      }

      yield async_read(*s->socket, BUF(s->readHeader), rh);

      if(ec) {
        if(ec == boost::system::errc::connection_reset ||
           ec == boost::system::errc::operation_canceled) {
          // No spamming about reset connections!
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_TRACE,
                    "Connection to {} reset. Ending connection.",
                    s->remoteId());
          return;
        }
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_LOCALERROR,
          "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
          "reading from socket: {} (read {} bytes)",
          s->remote_endpoint(),
          s->remoteId(),
          ec.message(),
          bytes_received);
        return;
      }

#ifdef ENABLE_DISTRAC
      {
        auto distrac = s->service.handle().distrac;
        if(distrac && s->readHeader.kind != PARAC_MESSAGE_ACK &&
           s->readHeader.kind != PARAC_MESSAGE_END) {
          parac_ev_recv_msg e{ s->readHeader.size + sizeof(s->readHeader),
                               s->remoteId(),
                               s->readHeader.kind,
                               s->readHeader.number };
          distrac_push(distrac, &e, PARAC_EV_RECV_MSG);
        }
      }
#endif

      if(s->readHeader.kind == PARAC_MESSAGE_ACK) {
        if(!handleReceivedACK(s->readHeader)) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "reading ACK for message id {}!",
            s->remote_endpoint(),
            s->remoteId(),
            s->readHeader.number);
          return;
        }
      } else if(s->readHeader.kind == PARAC_MESSAGE_FILE) {
        yield async_read(*s->socket, BUF(s->readFileHeader), rh);
        if(ec) {
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_LOCALERROR,
                    "Error in TCPConnection to endpoint {} (ID {}) readHandler "
                    "when reading "
                    "file header from message number {}! Error: {}",
                    s->remote_endpoint(),
                    s->remoteId(),
                    s->readHeader.number,
                    ec.message());
          return;
        }

        handleReceivedFileStart();

        while(s->readHeader.size > 0) {
          s->recvBuf.resize(
            std::min((unsigned long)REC_BUF_SIZE, s->readHeader.size));
          yield async_read(*s->socket, VBUF(s->recvBuf), rh);

          if(ec) {
            parac_log(
              PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
              "reading "
              "file chunk from file message number {}! Bytes left: {}. Error: "
              "{}",
              s->remote_endpoint(),
              s->remoteId(),
              s->readHeader.number,
              s->readHeader.size,
              ec.message());
            return;
          }
          s->readHeader.size -= bytes_received;
          handleReceivedFileChunk();
        }
        handleReceivedFile();
      } else if(s->readHeader.kind == PARAC_MESSAGE_END) {
        s->resumeMode = EndAfterShutdown;
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_TRACE,
                  "Connection End-Tag received in connection to {}",
                  s->remoteId());
        return;
      } else if(s->readHeader.kind == PARAC_MESSAGE_KEEPALIVE) {
        // Nothing to do, this message just keeps the connection alive.
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_TRACE,
                  "Receive Keepalive packet in TCPConnection to remote {} (ID: "
                  "{}). Message number: {}.",
                  s->remote_endpoint(),
                  s->remoteId(),
                  s->readHeader.number);
      } else {
        // Read rest of message, then pass on to handler function from compute
        // node.
        if(!s->compute_node) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "processing received message of kind {}, size {}, and number {}! "
            "Compute node not defined!",
            s->remote_endpoint(),
            s->remoteId(),
            s->readHeader.kind,
            s->readHeader.size,
            s->readHeader.number);
          return;
        }

        if(s->readHeader.size > MAX_BUF_SIZE) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "reading message of kind {}, size {}, and number {}! "
            "Too big, buffer would be larger than MAX_BUF_SIZE {}!",
            s->remote_endpoint(),
            s->remoteId(),
            s->readHeader.kind,
            s->readHeader.size,
            s->readHeader.number,
            bytes_received,
            MAX_BUF_SIZE);
          return;
        }
        s->recvBuf.resize(s->readHeader.size);
        yield async_read(*s->socket, VBUF(s->recvBuf), rh);

        if(ec || bytes_received != s->readHeader.size ||
           bytes_received != s->recvBuf.size()) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "reading message of kind {}, size {}, and number {}! "
            "Not enough bytes (only {}) read! Error: {}",
            s->remote_endpoint(),
            s->remoteId(),
            s->readHeader.kind,
            s->readHeader.size,
            s->readHeader.number,
            bytes_received,
            ec.message());
          return;
        }

        if(!handleReceivedMessage()) {
          // Connection is to be aborted!
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_TRACE,
                    "Receive message failed! Exiting connection to {}",
                    s->remoteId());
          return;
        }
      }
    }
  }
}

void
TCPConnection::writeHandler(boost::system::error_code ec,
                            size_t bytes_transferred) {
  if(shouldHandlerBeEnded()) {
    return;
  }

  using namespace boost::asio;
  auto wh = std::bind(&TCPConnection::writeHandler,
                      *this,
                      std::placeholders::_1,
                      std::placeholders::_2);

  auto s = state();
  if(!s) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Ending write handler because state was destructed!");
    return;
  }

  if(ec) {
    boost::system::error_code ec;
    s->socket->cancel(ec);
    s->socket->close(ec);
    return;
  }

  std::unique_lock lock(s->sendQueueMutex);

  SendQueueEntry* e = nullptr;
  if(!s->sendQueue.empty()) {
    e = &s->sendQueue.front();
  }

  if(s->compute_node) {
    s->compute_node->bytes_sent += bytes_transferred;
  }

  // Defined up here in order to be created when sending files. When it is
  // deleted, it has already been copied internally and carries on the reference
  // to this connection's state. This does not hold any state itself, it must
  // first be created using the proper constructor - this variable is nothing
  // but a workaround for switch & stackless coroutine variable declaration
  // issues.
  FileSender fileSender;

  reenter(s->writeCoro) {
    yield async_write(*s->socket, BUF(s->writeInitiatorMessage), wh);
    if(ec) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALERROR,
                "Error when sending initiator packet to endpoint {}! Error: {}",
                s->remote_endpoint(),
                ec.message());
      return;
    }

    while(true) {
      while(s->sendQueue.empty()) {
        s->currentlySending.clear();
        setKeepaliveTimer();
        yield;
      }
      setKeepaliveTimer();
      e = &s->sendQueue.front();

      assert(e);
#ifdef ENABLE_DISTRAC
      {
        auto distrac = s->service.handle().distrac;
        if(distrac && e->header.kind != PARAC_MESSAGE_ACK &&
           e->header.kind != PARAC_MESSAGE_END) {
          parac_ev_send_msg entry{ e->header.size + sizeof(e->header),
                                   s->remoteId(),
                                   e->header.kind,
                                   e->header.number };
          distrac_push(distrac, &entry, PARAC_EV_SEND_MSG);
        }
      }
#endif

      s->transmitMode = e->transmitMode;

      if(s->service.stopped())
        return;

      yield async_write(*s->socket, BUF(e->header), wh);

      if(ec == boost::system::errc::broken_pipe ||
         ec == boost::system::errc::connection_reset) {
        // Connection can be closed silently.
        s->resumeMode = EndAfterShutdown;
        return;
      }

      if(ec) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_LOCALERROR,
                  "Error when sending packet header to remote ID {}! Error: {}",
                  s->remoteId(),
                  ec.message());
        return;
      }

      if(e->header.kind != PARAC_MESSAGE_ACK &&
         e->header.kind != PARAC_MESSAGE_END &&
         e->header.kind != PARAC_MESSAGE_KEEPALIVE) {
        auto n = e->header.number;
        e->sent = std::chrono::steady_clock::now();
        s->sentBuffer.try_emplace(n, std::move(s->sendQueue.front()));

        parac_log(PARAC_COMMUNICATOR,
                  PARAC_TRACE,
                  "Send message of kind {} with id {} to remote {} and waiting "
                  "for ACK.",
                  e->header.kind,
                  e->header.number,
                  s->remoteId());
      }

      if(e->header.kind == PARAC_MESSAGE_ACK ||
         e->header.kind == PARAC_MESSAGE_END ||
         e->header.kind == PARAC_MESSAGE_KEEPALIVE) {
        // Nothing has to be done, message already finished.
      } else if(e->header.kind == PARAC_MESSAGE_FILE) {
        assert(e);

        fileSender = FileSender(
          e->file().path,
          *s->socket,
          std::bind(
            &TCPConnection::writeHandler, *this, std::placeholders::_1, 0),
          s->service);
        // Handles the sending internally and calls the callback to this
        // function when it's done. If there is some error, the connection is
        // dropped and re-established.
        yield fileSender.send();
      } else {
        if(e->message().data_is_inline) {
          assert(e->header.size <= PARAC_MESSAGE_INLINE_DATA_SIZE);
          yield async_write(
            *s->socket, buffer(e->message().inline_data, e->header.size), wh);
        } else {
          yield async_write(
            *s->socket, buffer(e->message().data, e->header.size), wh);
        }

        if(ec) {
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_LOCALERROR,
                    "Error when sending message to remote ID {}! Error: {}",
                    s->remoteId(),
                    ec.message());
          return;
        }
      }

      if(e->header.kind != PARAC_MESSAGE_KEEPALIVE) {
        s->service.removeOutgoingMessageFromCounter();
      }

      s->sendQueue.pop();
    }
  }
}

#undef BUF
#undef VBUF

#include <boost/asio/unyield.hpp>

void
TCPConnection::setKeepaliveTimer() {
  if(auto s = state()) {
    s->keepaliveTimer.expires_from_now(
      std::chrono::milliseconds(s->service.keepaliveIntervalMS()));
    TCPConnection weakThis = TCPConnection(*this, WeakStatePtr(s));
    s->keepaliveTimer.async_wait(
      [weakThis{ std::move(weakThis) }](
        const boost::system::error_code& ec) mutable {
        if(!ec) {
          // Timer expired! A message of type PARAC_MESSAGE_KEEPALIVE must be
          // sent in order to keep the peer readHandler from running into a
          // timeout!
          parac_message_wrapper msg;
          msg.kind = PARAC_MESSAGE_KEEPALIVE;
          weakThis.send(msg);
        }
      });
  }
}

std::ostream&
operator<<(std::ostream& o, TCPConnection::ResumeMode resumeMode) {
  return o << ConnectionResumeModeToStr(resumeMode);
}
}
