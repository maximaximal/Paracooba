#include "tcp_connection.hpp"
#include "file_sender.hpp"
#include "message_send_queue.hpp"
#include "packet.hpp"
#include "paracooba/broker/broker.h"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/random.h"
#include "paracooba/common/status.h"
#include "paracooba/communicator/communicator.h"
#include "service.hpp"
#include "tcp_connection_initiator.hpp"
#include "transmit_mode.hpp"

#include <algorithm>
#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <limits>
#include <thread>
#include <type_traits>
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
#include <random>

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
#define MAX_BUF_SIZE 100'000'000u

namespace parac::communicator {
struct TCPConnection::InitiatorMessage {
  parac_id sender_id;
  uint16_t tcp_port;
};

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

  ~State() {
    if(!service.stopped()) {
      parac_log(
        PARAC_COMMUNICATOR,
        PARAC_TRACE,
        "Connection to remote {} ended with resume mode {}. Was initiator: {}",
        remoteId(),
        resumeMode,
        connectionTry >= 0);

      if(sendQueue) {
        sendQueue->deregisterNotificationCB();
      }
    }

    if(!service.stopped()) {
      switch(resumeMode) {
        case RestartAfterShutdown: {
          if(connectionTry >= 0 &&
             connectionTry < service.connectionRetries()) {

            // Reconnects only happen if this side of the connection is the
            // original initiating side. The other side will end the connection.

            size_t timeout = parac_size_normal_distribution(1200, 200);

            parac_log(PARAC_COMMUNICATOR,
                      PARAC_TRACE,
                      "Reconnect to endpoint {} (remote id {}), try {} after "
                      "random timeout ({}ms)",
                      cachedRemoteEndpoint,
                      remoteId(),
                      connectionTry,
                      timeout);

            TCPConnectionInitiator* initiator = new TCPConnectionInitiator(
              service, cachedRemoteEndpoint, nullptr, ++connectionTry, true);

            parac_timeout* timeout_handle =
              service.setTimeout(timeout, initiator, [](parac_timeout* t) {
                TCPConnectionInitiator* initiator =
                  static_cast<TCPConnectionInitiator*>(t->expired_userdata);
                initiator->run();
                delete initiator;
              });
            assert(timeout_handle);
          }
          break;
        }
        case EndAfterShutdown:
          // Do nothing, as the connection should just be ended.
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
  std::atomic_bool killed = false;

  std::atomic<TransmitMode> transmitMode = TransmitMode::Init;
  ResumeMode resumeMode = RestartAfterShutdown;

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

  std::atomic_flag currentlySending = true;
  boost::asio::ip::tcp::endpoint cachedRemoteEndpoint;

  boost::asio::steady_timer readTimer;
  boost::asio::steady_timer keepaliveTimer;

  std::shared_ptr<MessageSendQueue> sendQueue;
};

template<typename S, typename B, typename CB>
void
TCPConnection::async_write(S& socket, B&& buffer, CB& cb) {
  boost::asio::async_write(socket, buffer, cb);
}

template<typename S, typename B, typename CB>
void
TCPConnection::async_read(S& socket, B&& buffer, CB cb, parac_id remoteId) {
  auto timeout_handler = [cb, remoteId](const boost::system::error_code& ec) {
    if(!ec) {
      // The read timed out! Call the CB with a timeout error.
      parac_log(PARAC_COMMUNICATOR,
                PARAC_GLOBALWARNING,
                "Connection to {} run into timeout!",
                remoteId);

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
  int connectionTry) {
  m_stateStore =
    std::make_shared<State>(service, std::move(socket), connectionTry);

  // Set at beginning, only unset if write_handler yields. Set again at
  // send(SendQueueItem&).
  // state()->currentlySending.test_and_set();

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
      if(s->transmitMode != TransmitMode::End && s.use_count() == 1) {
        // This is the last connection, so this is also the last one having a
        // reference to the state. This is a clean shutdown of a connection, the
        // socket will be ended. To differentiate between this clean shutdown
        // and a dirty one, an EndToken must be transmitted. This is the last
        // action.
        TCPConnection(*this).send(MessageSendQueue::EndTag{});
      }
    }
  }
}

void
TCPConnection::setResumeMode(ResumeMode mode) {
  auto s = state();
  if(s)
    s->resumeMode = mode;
}

template<typename S, typename T>
inline static bool
SendToMessageSendQueue(S s, T&& v) {
  if(!s)
    return false;
  if(!s->sendQueue)
    return false;

  if constexpr(std::is_rvalue_reference<T>())
    s->sendQueue->send(std::forward(v));
  else
    s->sendQueue->send(v);

  return true;
}

void
TCPConnection::send(parac_message&& message) {
  parac_message m(message);
  if(!SendToMessageSendQueue(state(), std::move(message))) {
    if(m.cb) {
      m.cb(&m, PARAC_CONNECTION_CLOSED);
      m.cb(&m, PARAC_TO_BE_DELETED);
    }
  }
}
void
TCPConnection::send(parac_file&& file) {
  parac_file f(file);
  if(!SendToMessageSendQueue(state(), std::move(file))) {
    if(f.cb) {
      f.cb(&f, PARAC_CONNECTION_CLOSED);
      f.cb(&f, PARAC_TO_BE_DELETED);
    }
  }
}
void
TCPConnection::send(MessageSendQueue::EndTag&& end) {
  SendToMessageSendQueue(state(), std::move(end));
}
void
TCPConnection::send(parac_message& message) {
  if(!SendToMessageSendQueue(state(), message)) {
    if(message.cb) {
      message.cb(&message, PARAC_CONNECTION_CLOSED);
      message.cb(&message, PARAC_TO_BE_DELETED);
    }
  }
}
void
TCPConnection::send(parac_file& file) {
  if(!SendToMessageSendQueue(state(), file)) {
    if(file.cb) {
      file.cb(&file, PARAC_CONNECTION_CLOSED);
      file.cb(&file, PARAC_TO_BE_DELETED);
    }
  }
}
void
TCPConnection::send(MessageSendQueue::EndTag& end) {
  SendToMessageSendQueue(state(), end);
}
void
TCPConnection::sendACK(uint32_t id, parac_status status) {
  auto s = state();
  assert(s);
  if(!s->sendQueue)
    return;
  s->sendQueue->sendACK(id, status);
}

bool
TCPConnection::conditionallyTriggerWriteHandler() {
  auto s = state();
  if(!s)
    return false;

  if(!s->currentlySending.test_and_set()) {
    auto weakConn = TCPConnection(*this, WeakStatePtr(s));
    if(s->service.ioContextThreadId() == std::this_thread::get_id()) {
      weakConn.writeHandler(boost::system::error_code(), 0);
    } else {
      // The Socket is not thread-safe, so when sending from some other thread,
      // the send must be issued over the ioContext thread by posting work onto
      // the context.
      s->service.ioContext().post([weakConn]() mutable {
        weakConn.writeHandler(boost::system::error_code(), 0);
      });
    }
  }
  return true;
}

bool
TCPConnection::kill() {
  auto s = state();
  if(!s) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Cannot kill connection to because it is already dead!");
    return false;
  }
  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Connection to {} scheduled for kill!",
            s->remoteId());
  s->killed = true;
  conditionallyTriggerWriteHandler();
  return true;
}

bool
TCPConnection::wasInitiator() {
  auto s = state();
  assert(s);
  return s->connectionTry >= 0;
}

void
TCPConnection::injectMessageSendQueue(
  std::shared_ptr<MessageSendQueue> sendQueue) {
  auto s = state();
  assert(s);
  s->sendQueue = sendQueue;
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

  s->remoteIdCache = init.sender_id;

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

  auto sendQueue = s->service.getMessageSendQueueForRemoteId(init.sender_id);
  TCPConnection weakConn(*this, WeakStatePtr(state()));

  std::stringstream connStr;
  auto a = s->socket->remote_endpoint().address();
  connStr << (a.is_v6() ? "[" : "") << a.to_string() << (a.is_v6() ? "]" : "")
          << ':' << init.tcp_port;

  auto [compute_node, send_queue] = sendQueue->registerNotificationCB(
    [weakConn](MessageSendQueue::Event e) mutable -> bool {
      switch(e) {
        case MessageSendQueue::MessagesAvailable:
          return weakConn.conditionallyTriggerWriteHandler();
        case MessageSendQueue::KillConnection:
          return weakConn.kill();
      }
      return true;
    },
    connStr.str(),
    s->connectionTry >= 0);

  if(!compute_node) {
    s->resumeMode = EndAfterShutdown;
    return false;
  }

  s->compute_node = compute_node;
  s->sendQueue = send_queue;

  // Send queue was now set! Maybe there are already messages from other
  // modules. Send these.
  if(!send_queue->empty()) {
    conditionallyTriggerWriteHandler();
  }

  return true;
}

bool
TCPConnection::handleReceivedACK(const PacketHeader& ack) {
  auto s = state();
  assert(s);
  assert(s->sendQueue);
  return s->sendQueue->handleACK(ack);
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

  // Data can be zero in very specific error scenarios.
  assert(msg.length == 0 || (msg.length > 0 && msg.data));

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

  if(!s->readFileOstream->o) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_FATAL,
              "Could not open file ostream for file {} to path {} for "
              "receiving file from remote {}! Error: {}",
              name,
              p,
              s->remoteId(),
              strerror(errno));
  }

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
    s->cachedRemoteEndpoint = s->socket->remote_endpoint();

    yield async_read(
      *s->socket, BUF(s->readInitiatorMessage), rh, s->remoteId());

    if(ec) {
      if(ec == boost::system::errc::connection_reset ||
         ec == boost::system::errc::operation_canceled) {
        return;
      }
      parac_log(
        PARAC_COMMUNICATOR,
        PARAC_LOCALERROR,
        "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
        "reading initiator message: {}",
        s->remote_endpoint(),
        s->remoteId(),
        ec.message());
      return;
    }

    if(!handleInitiatorMessage(s->readInitiatorMessage)) {
      // This only means that the connection was already created.
      return;
    }

    if(s->sendQueue)
      s->sendQueue->notifyOfRead();

    while(true) {
      if(ec) {
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_LOCALERROR,
          "Error in TCPConnection to endpoint {} (ID {}) readHandler before "
          "reading: {}",
          s->remote_endpoint(),
          s->remoteId(),
          ec.message());
        return;
      }

      yield async_read(*s->socket, BUF(s->readHeader), rh, s->remoteId());

      if(ec) {
        if(ec == boost::system::errc::connection_reset ||
           ec == boost::system::errc::operation_canceled) {
          // No spamming about reset connections!
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_TRACE,
                    "Connection to {} reset. Error: {}. Ending connection.",
                    s->remoteId(),
                    ec.message());
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

      if(s->sendQueue)
        s->sendQueue->notifyOfRead();

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
            PARAC_LOCALWARNING,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "reading ACK for message id {}!",
            s->remote_endpoint(),
            s->remoteId(),
            s->readHeader.number);
          return;
        }
      } else if(s->readHeader.kind == PARAC_MESSAGE_FILE) {
        yield async_read(*s->socket, BUF(s->readFileHeader), rh, s->remoteId());
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
          yield async_read(*s->socket, VBUF(s->recvBuf), rh, s->remoteId());

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

          if(s->sendQueue)
            s->sendQueue->notifyOfRead();

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
        yield async_read(*s->socket, VBUF(s->recvBuf), rh, s->remoteId());

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

        if(s->sendQueue)
          s->sendQueue->notifyOfRead();
      }

      if(s->killed) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_TRACE,
                  "Connection to {} killed!",
                  s->remoteId());
        return;
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

  MessageSendQueue::EntryRef e;
  if(s->sendQueue && !s->sendQueue->empty()) {
    e = s->sendQueue->front();
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
      while((!s->sendQueue || s->sendQueue->empty()) && !s->killed) {
        s->currentlySending.clear();
        setKeepaliveTimer();
        yield;
      }

      if(s->killed) {
        // Killed means that the normal way of using the send queue is not valid
        // anymore! Some other connection is using that queue. Directly send the
        // end message and quit the write handler for good.

        PacketHeader endHeader;
        endHeader.kind = PARAC_MESSAGE_END;
        endHeader.size = 0;
        endHeader.originator = s->service.id();
        endHeader.number =
          std::numeric_limits<decltype(endHeader.number)>::max();
        boost::asio::write(*s->socket, BUF(endHeader));

        while(s->killed) {
          yield;
        }
      }
      setKeepaliveTimer();
      e = s->sendQueue->front();

      assert(!s->sendQueue->empty());

      assert(e.header);
#ifdef ENABLE_DISTRAC
      {
        auto distrac = s->service.handle().distrac;
        if(distrac && e.header->kind != PARAC_MESSAGE_ACK &&
           e.header->kind != PARAC_MESSAGE_END) {
          parac_ev_send_msg entry{ e.header->size + sizeof(*e.header),
                                   s->remoteId(),
                                   e.header->kind,
                                   e.header->number };
          distrac_push(distrac, &entry, PARAC_EV_SEND_MSG);
        }
      }
#endif

      s->transmitMode = e.transmitMode;

      if(s->service.stopped())
        return;

      assert(s->sendQueue);
      assert(!s->sendQueue->empty());
      assert(e.header);

      yield async_write(*s->socket, BUF((*e.header)), wh);

      if(s->killed)
        continue;

      if(ec == boost::system::errc::broken_pipe ||
         ec == boost::system::errc::connection_reset) {
        // Connection can be closed silently.
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

      assert(s->sendQueue);

      if(s->sendQueue->empty()) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_LOCALERROR,
                  "Send queue to remote id {} empty even though coroutine says "
                  "it should be empty! Ending connection!",
                  s->remoteId());
        yield return;
      }

      assert(!s->sendQueue->empty());
      assert(e.header);

      if(e.header->kind == PARAC_MESSAGE_ACK ||
         e.header->kind == PARAC_MESSAGE_END ||
         e.header->kind == PARAC_MESSAGE_KEEPALIVE) {
        // Nothing has to be done, message already finished.
      } else if(e.header->kind == PARAC_MESSAGE_FILE) {
        fileSender = FileSender(
          e.file().path,
          *s->socket,
          std::bind(
            &TCPConnection::writeHandler, *this, std::placeholders::_1, 0),
          s->service);
        // Handles the sending internally and calls the callback to this
        // function when it's done. If there is some error, the connection is
        // dropped and re-established.
        yield fileSender.send();

        if(s->killed)
          continue;
      } else {
        if(e.message().data_is_inline) {
          assert(e.header->size <= PARAC_MESSAGE_INLINE_DATA_SIZE);
          yield async_write(
            *s->socket, buffer(e.message().inline_data, e.header->size), wh);

          if(s->killed)
            continue;
        } else {
          yield async_write(
            *s->socket, buffer(e.message().data, e.header->size), wh);

          if(s->killed)
            continue;
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

      if(s->killed)
        continue;
      s->sendQueue->popFromQueued();
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
