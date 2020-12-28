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
#include <boost/asio/io_context.hpp>
#include <boost/asio/placeholders.hpp>
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

#include <distrac_paracooba.h>

#define REC_BUF_SIZE 4096u
#define MAX_BUF_SIZE 10000000u

namespace parac::communicator {
struct TCPConnection::EndTag {};
struct TCPConnection::ACKTag {};

struct TCPConnection::InitiatorMessage {
  parac_id sender_id;
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

struct TCPConnection::State {
  explicit State(Service& service,
                 std::unique_ptr<boost::asio::ip::tcp::socket> socket,
                 int connectionTry)
    : service(service)
    , socket(std::move(socket))
    , steadyTimer(service.ioContext())
    , connectionTry(connectionTry)
    , writeInitiatorMessage({ service.handle().id }) {
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
    , steadyTimer(service.ioContext())
    , connectionTry(connectionTry)
    , writeInitiatorMessage({ service.handle().id })
    , sendQueue(std::move(ptr->queue))
    , sentBuffer(std::move(ptr->map)) {}

  ~State() {
    if(compute_node) {
      compute_node->communicator_free(compute_node);
      compute_node = nullptr;
    }
    if(service.ioContext().stopped()) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "Connection ended with resume mode {}.",
                resumeMode);

      switch(resumeMode) {
        case RestartAfterShutdown: {
          if(connectionTry >= 0 &&
             connectionTry < service.connectionRetries()) {
            // Reconnects only happen if this side of the connection is the
            // original initiating side. The other side will end the connection.

            parac_log(PARAC_COMMUNICATOR,
                      PARAC_TRACE,
                      "Reconnect to endpoint {} (remote id {}), try {}.",
                      socket->remote_endpoint(),
                      remoteId(),
                      connectionTry);
            TCPConnectionInitiator initiator(
              service, socket->remote_endpoint(), nullptr, ++connectionTry);

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
  boost::asio::steady_timer steadyTimer;
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

  parac_id remoteId() const {
    if(compute_node)
      return compute_node->id;
    return 0;
  }

  TCPConnectionPayloadPtr generatePayload() {
    return TCPConnectionPayloadPtr(
      new TCPConnectionPayload(std::move(sendQueue), std::move(sentBuffer)),
      &TCPConnectionPayloadDestruct);
  }

  Lifecycle lifecycle = Initializing;
  std::atomic_flag currentlySending = true;

  std::queue<SendQueueEntry> sendQueue;
  using SentBuffer = std::map<decltype(PacketHeader::number), SendQueueEntry>;
  SentBuffer sentBuffer;
};

TCPConnection::TCPConnection(
  Service& service,
  std::unique_ptr<boost::asio::ip::tcp::socket> socket,
  int connectionTry,
  TCPConnectionPayloadPtr ptr) {
  if(ptr) {
    m_state = std::make_shared<State>(
      service, std::move(socket), connectionTry, std::move(ptr));
  } else {
    m_state =
      std::make_shared<State>(service, std::move(socket), connectionTry);
  }

  // Set at beginning, only unset if write_handler yields. Set again at
  // send(SendQueueItem&).
  m_state->currentlySending.test_and_set();

  writeHandler(boost::system::error_code(), 0);
  readHandler(boost::system::error_code(), 0);
}

TCPConnection::~TCPConnection() {
  /*
  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "TCPConnection to {} destruct. State use count is {}. Position in "
            "readHandler {}, writeHandler {}.",
            m_state->remoteId(),
            m_state.use_count(),
            *reinterpret_cast<int*>(
              &m_state->readCoro),// Hacky reading for debugging purposes.
            *reinterpret_cast<int*>(&m_state->writeCoro));
  */

  if(!m_state->service.ioContext().stopped()) {
    if(m_state->transmitMode != TransmitEnd && m_state.use_count() == 1) {
      // This is the last connection, so this is also the last one having a
      // reference to the state. This is a clean shutdown of a connection, the
      // socket will be ended. To differentiate between this clean shutdown
      // and a dirty one, an EndToken must be transmitted. This is the last
      // action.
      TCPConnection(*this).send(EndTag{});
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
  {
    std::lock_guard lock(m_state->sendQueueMutex);
    if(e.header.kind != PARAC_MESSAGE_ACK) {
      e.header.number = m_state->sendMessageNumber++;
    }
    m_state->sendQueue.emplace(std::move(e));
  }
  if(!m_state->currentlySending.test_and_set()) {
    writeHandler(boost::system::error_code(), 0);
  }
}

bool
TCPConnection::shouldHandlerBeEnded() {
  if(m_state->compute_node) {
    auto s = m_state->compute_node->state;
    if(s == PARAC_COMPUTE_NODE_TIMEOUT || s == PARAC_COMPUTE_NODE_EXITED) {
      return true;
    }
  }
  if(m_state->service.ioContext().stopped()) {
    return true;
  }
  return false;
}

bool
TCPConnection::handleInitiatorMessage(const InitiatorMessage& init) {
  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Handle initiator message on node {} from node {}.",
            m_state->service.handle().id,
            init.sender_id);

  if(init.sender_id == m_state->service.handle().id) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_DEBUG,
              "Not accepting connection from same node ({}).",
              init.sender_id);
    return false;
  }

  auto compute_node_store = m_state->service.handle()
                              .modules[PARAC_MOD_BROKER]
                              ->broker->compute_node_store;

  TCPConnection* conn = new TCPConnection(*this);

  m_state->compute_node = compute_node_store->get_with_connection(
    compute_node_store,
    init.sender_id,
    &TCPConnection::compute_node_free_func,
    conn,
    &TCPConnection::compute_node_send_message_to_func,
    &TCPConnection::compute_node_send_file_to_func);

  if(!m_state->compute_node) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Error when creating compute node {}!",
              init.sender_id);
    delete conn;
    return false;
  }

  return true;
}

bool
TCPConnection::handleReceivedACK(const PacketHeader& ack) {
  State::SentBuffer::iterator it;
  {
    std::unique_lock lock(m_state->sendQueueMutex);
    it = m_state->sentBuffer.find(ack.number);
    if(it == m_state->sentBuffer.end()) {
      return false;
    }
  }
  auto& sentItem = it->second;
  sentItem(ack.ack_status);
  sentItem(PARAC_TO_BE_DELETED);

  auto distrac = m_state->service.handle().distrac;
  if(distrac) {
    parac_ev_send_msg_ack e{ m_state->remoteId(),
                             sentItem.header.kind,
                             sentItem.header.number };
    distrac_push(distrac, &e, PARAC_EV_SEND_MSG_ACK);
  }

  {
    std::unique_lock lock(m_state->sendQueueMutex);
    m_state->sentBuffer.erase(it);
  }
  return true;
}

void
TCPConnection::handleReceivedMessage() {
  parac_message msg;
  struct data {
    parac_status status;
    bool returned = false;
  };
  data d;

  msg.kind = m_state->readHeader.kind;
  msg.length = m_state->readHeader.size;
  msg.data = static_cast<char*>(m_state->recvBuf.data());
  msg.data_to_be_freed = false;
  msg.userdata = &d;
  msg.originator_id = m_state->readHeader.originator;
  msg.origin = m_state->compute_node;
  msg.cb = [](parac_message* msg, parac_status status) {
    data* d = static_cast<data*>(msg->userdata);
    d->status = status;
    d->returned = true;
  };
  assert(msg.data);

  assert(m_state->compute_node->receive_message_from);
  m_state->compute_node->receive_message_from(m_state->compute_node, &msg);

  // Callback must be called immediately! This gives the status that is
  // then passed back.
  assert(d.returned);

  sendACK(m_state->readHeader.number, d.status);
}

void
TCPConnection::handleReceivedFileStart() {
  std::string name = m_state->readFileHeader.name;

  boost::filesystem::path p(m_state->service.temporaryDirectory());

  if(m_state->readHeader.originator != m_state->service.handle().id) {
    p /= std::to_string(m_state->readHeader.originator);
    boost::filesystem::create_directory(p);
  }
  p /= name;

  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Start receiving file \"{}\" from {} to {}",
            name,
            m_state->remoteId(),
            p);

  m_state->readFileOstream = std::make_unique<State::FileOstream>(
    State::FileOstream{ std::ofstream{ p.string().c_str(), std::ios::binary },
                        p.string(),
                        m_state->readFileHeader.name });

  assert(m_state->readFileOstream->o);
  assert(m_state->readFileOstream->o.is_open());
}

void
TCPConnection::handleReceivedFileChunk() {
  auto& o = m_state->readFileOstream;
  assert(o);
  o->o.write(m_state->recvBuf.data(), m_state->recvBuf.size());
}

void
TCPConnection::handleReceivedFile() {
  parac_file file;
  struct data {
    parac_status status;
    bool returned = false;
  };
  data d;

  m_state->readFileOstream->o.flush();

  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Finished receiving file to \"{}\" from {} with originator {}. "
            "Wrote {} bytes.",
            m_state->readFileOstream->path,
            m_state->remoteId(),
            m_state->readHeader.originator,
            m_state->readFileOstream->o.tellp());

  file.cb = [](parac_file* file, parac_status status) {
    data* d = static_cast<data*>(file->userdata);
    d->status = status;
    d->returned = true;
  };
  file.userdata = &d;
  file.path = m_state->readFileOstream->path.c_str();
  file.originator = m_state->readHeader.originator;

  assert(m_state->compute_node);
  assert(m_state->compute_node->receive_file_from);

  m_state->compute_node->receive_file_from(m_state->compute_node, &file);

  // Callback must be called immediately! This gives the status that is
  // then passed back.
  assert(d.returned);

  sendACK(m_state->readHeader.number, d.status);
}

void
TCPConnection::compute_node_free_func(parac_compute_node* n) {
  assert(n);
  n->send_file_to = nullptr;
  n->send_message_to = nullptr;
  if(n->communicator_userdata) {
    TCPConnection* conn = static_cast<TCPConnection*>(n->communicator_userdata);
    if(conn->m_state->compute_node == n) {
      conn->m_state->compute_node = nullptr;
    }
    conn->send(EndTag());
    delete conn;
    n->communicator_userdata = nullptr;
  }
}

void
TCPConnection::compute_node_send_message_to_func(parac_compute_node* n,
                                                 parac_message* msg) {
  assert(n);
  assert(n->communicator_userdata);
  TCPConnection* conn = static_cast<TCPConnection*>(n->communicator_userdata);
  conn->send(*msg);
}

void
TCPConnection::compute_node_send_file_to_func(parac_compute_node* n,
                                              parac_file* file) {
  assert(n);
  assert(n->communicator_userdata);
  TCPConnection* conn = static_cast<TCPConnection*>(n->communicator_userdata);
  conn->send(*file);
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

  if(shouldHandlerBeEnded()) {
    return;
  }

  if(ec == boost::asio::error::eof) {
    // End of file detected. This means the connection should shut down
    // cleanly, without bloating the exception handling in the internal loop.
    // Eventual reconnects are handled by destructors.
    return;
  }

  if(m_state->compute_node) {
    m_state->compute_node->bytes_received += bytes_received;
  }

  reenter(&m_state->readCoro) {
    yield async_read(*m_state->socket, BUF(m_state->readInitiatorMessage), rh);

    if(ec) {
      parac_log(
        PARAC_COMMUNICATOR,
        PARAC_LOCALERROR,
        "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
        "reading initiator message: {}",
        m_state->socket->remote_endpoint(),
        m_state->remoteId(),
        ec);
      return;
    }

    if(!handleInitiatorMessage(m_state->readInitiatorMessage)) {
      return;
    }

    while(true) {
      if(ec) {
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_LOCALERROR,
          "Error in TCPConnection to endpoint {} (ID {}) readHandler before "
          "reading: {}",
          m_state->socket->remote_endpoint(),
          m_state->remoteId(),
          ec);
        return;
      }

      yield async_read(*m_state->socket, BUF(m_state->readHeader), rh);

      if(ec) {
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_LOCALERROR,
          "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
          "reading from socket: {} (read {} bytes)",
          m_state->socket->remote_endpoint(),
          m_state->remoteId(),
          ec.message(),
          bytes_received);
        return;
      }

      {
        auto distrac = m_state->service.handle().distrac;
        if(distrac && m_state->readHeader.kind != PARAC_MESSAGE_ACK &&
           m_state->readHeader.kind != PARAC_MESSAGE_END) {
          parac_ev_recv_msg e{ m_state->readHeader.size +
                                 sizeof(m_state->readHeader),
                               m_state->remoteId(),
                               m_state->readHeader.kind,
                               m_state->readHeader.number };
          distrac_push(distrac, &e, PARAC_EV_RECV_MSG);
        }
      }

      if(m_state->readHeader.kind == PARAC_MESSAGE_ACK) {
        if(!handleReceivedACK(m_state->readHeader)) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "reading ACK!",
            m_state->socket->remote_endpoint(),
            m_state->remoteId());
          return;
        }
      } else if(m_state->readHeader.kind == PARAC_MESSAGE_FILE) {
        yield async_read(*m_state->socket, BUF(m_state->readFileHeader), rh);
        if(ec) {
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_LOCALERROR,
                    "Error in TCPConnection to endpoint {} (ID {}) readHandler "
                    "when reading "
                    "file header from message number {}! Error: {}",
                    m_state->socket->remote_endpoint(),
                    m_state->remoteId(),
                    m_state->readHeader.number,
                    ec.message());
          return;
        }

        handleReceivedFileStart();

        while(m_state->readHeader.size > 0) {
          m_state->recvBuf.resize(
            std::min((unsigned long)REC_BUF_SIZE, m_state->readHeader.size));
          yield async_read(*m_state->socket, VBUF(m_state->recvBuf), rh);

          if(ec) {
            parac_log(
              PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
              "reading "
              "file chunk from file message number {}! Bytes left: {}. Error: "
              "{}",
              m_state->socket->remote_endpoint(),
              m_state->remoteId(),
              m_state->readHeader.number,
              m_state->readHeader.size,
              ec.message());
            return;
          }
          m_state->readHeader.size -= bytes_received;
          handleReceivedFileChunk();
        }
        handleReceivedFile();
      } else if(m_state->readHeader.kind == PARAC_MESSAGE_END) {
        m_state->resumeMode = EndAfterShutdown;
        return;
      } else {
        // Read rest of message, then pass on to handler function from compute
        // node.
        if(!m_state->compute_node) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "processing received message of kind {}, size {}, and number {}! "
            "Compute node not defined!",
            m_state->socket->remote_endpoint(),
            m_state->remoteId(),
            m_state->readHeader.kind,
            m_state->readHeader.size,
            m_state->readHeader.number);
          return;
        }

        if(m_state->readHeader.size > MAX_BUF_SIZE) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "reading message of kind {}, size {}, and number {}! "
            "Too big, buffer would be larger than MAX_BUF_SIZE {}!",
            m_state->socket->remote_endpoint(),
            m_state->remoteId(),
            m_state->readHeader.kind,
            m_state->readHeader.size,
            m_state->readHeader.number,
            bytes_received,
            MAX_BUF_SIZE);
          return;
        }
        m_state->recvBuf.resize(m_state->readHeader.size);
        yield async_read(*m_state->socket, VBUF(m_state->recvBuf), rh);

        if(ec || bytes_received != m_state->readHeader.size ||
           bytes_received != m_state->recvBuf.size()) {
          parac_log(
            PARAC_COMMUNICATOR,
            PARAC_LOCALERROR,
            "Error in TCPConnection to endpoint {} (ID {}) readHandler when "
            "reading message of kind {}, size {}, and number {}! "
            "Not enough bytes (only {}) read! Error: {}",
            m_state->socket->remote_endpoint(),
            m_state->remoteId(),
            m_state->readHeader.kind,
            m_state->readHeader.size,
            m_state->readHeader.number,
            bytes_received,
            ec.message());
          return;
        }

        handleReceivedMessage();
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

  std::unique_lock lock(m_state->sendQueueMutex);

  SendQueueEntry* e = nullptr;
  if(!m_state->sendQueue.empty()) {
    e = &m_state->sendQueue.front();
  }

  if(m_state->compute_node) {
    m_state->compute_node->bytes_sent += bytes_transferred;
  }

  // Defined up here in order to be created when sending files. When it is
  // deleted, it has already been copied internally and carries on the reference
  // to this connection's state.
  std::unique_ptr<FileSender> sender;

  reenter(m_state->writeCoro) {
    yield async_write(
      *m_state->socket, BUF(m_state->writeInitiatorMessage), wh);
    if(ec) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALERROR,
                "Error when sending initiator packet to endpoint {}! Error: {}",
                m_state->socket->remote_endpoint(),
                ec.message());
      return;
    }

    while(true) {
      while(m_state->sendQueue.empty()) {
        m_state->currentlySending.clear();
        yield;
      }
      e = &m_state->sendQueue.front();

      assert(e);
      {
        auto distrac = m_state->service.handle().distrac;
        if(distrac && e->header.kind != PARAC_MESSAGE_ACK &&
           e->header.kind != PARAC_MESSAGE_END) {
          parac_ev_send_msg entry{ e->header.size + sizeof(e->header),
                                   m_state->remoteId(),
                                   e->header.kind,
                                   e->header.number };
          distrac_push(distrac, &entry, PARAC_EV_SEND_MSG);
        }
      }

      m_state->transmitMode = e->transmitMode;
      yield async_write(*m_state->socket, BUF(e->header), wh);

      if(ec) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_LOCALERROR,
                  "Error when sending packet header to remote ID {}! Error: {}",
                  m_state->remoteId(),
                  ec.message());
        return;
      }

      if(e->header.kind == PARAC_MESSAGE_ACK ||
         e->header.kind == PARAC_MESSAGE_END) {
        // Nothing has to be done, message already finished.
      } else if(e->header.kind == PARAC_MESSAGE_FILE) {
        assert(e);

        sender =
          std::make_unique<FileSender>(e->file().path,
                                       e->file().originator,
                                       *m_state->socket,
                                       std::bind(&TCPConnection::writeHandler,
                                                 *this,
                                                 boost::system::error_code(),
                                                 0));
        // Handles the sending internally and calls the callback to this
        // function when it's done. If there is some error, the connection is
        // dropped and re-established.
        yield sender->send();
      } else {
        if(e->message().data_is_inline) {
          assert(e->header.size <= PARAC_MESSAGE_INLINE_DATA_SIZE);
          yield async_write(
            *m_state->socket,
            const_buffer(e->message().inline_data, e->header.size),
            wh);
        } else {
          yield async_write(*m_state->socket,
                            const_buffer(e->message().data, e->header.size),
                            wh);
        }

        if(ec) {
          parac_log(PARAC_COMMUNICATOR,
                    PARAC_LOCALERROR,
                    "Error when sending message to remote ID {}! Error: {}",
                    m_state->remoteId(),
                    ec.message());
          return;
        }
      }

      if(e->header.kind != PARAC_MESSAGE_ACK &&
         e->header.kind != PARAC_MESSAGE_END) {
        auto n = e->header.number;
        e->sent = std::chrono::steady_clock::now();
        m_state->sentBuffer.try_emplace(n,
                                        std::move(m_state->sendQueue.front()));
      }
      m_state->sendQueue.pop();
    }
  }
}

#undef BUF
#undef VBUF

#include <boost/asio/unyield.hpp>

std::ostream&
operator<<(std::ostream& o, TCPConnection::ResumeMode resumeMode) {
  return o << ConnectionResumeModeToStr(resumeMode);
}
}
