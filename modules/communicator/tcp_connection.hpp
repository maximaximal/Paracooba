#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>

#include <boost/asio/ip/tcp.hpp>

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

#include "message_send_queue.hpp"

struct parac_message;
struct parac_file;
struct parac_compute_node;

namespace boost::system {
class error_code;
}

namespace parac::communicator {
class Service;
class PacketHeader;

/** @brief Class representing a connection between two compute nodes.
 *
 * This class handles the complete connection live-cycle. Connections are
 * established over TCP sockets. If a socket is ended, it is tried to reconnect
 * to the remote host.
 */
class TCPConnection {
  public:
  struct State;
  using WeakStatePtr = std::weak_ptr<State>;
  using SharedStatePtr = std::shared_ptr<State>;

  enum ResumeMode { EndAfterShutdown, RestartAfterShutdown };

  /** @brief Initialize a paracooba connection on an opened socket.
   * @param service The Service to use.
   * @param service socket Initialized socket to capture for this connection
   * (from either TCPConnectionInitiator or TCPAcceptor).
   * @param connectionTry Number of retries to establish connection (>0) when
   * used from TCPConnectionInitiator, -1 when initialized from TCPAcceptor.
   */
  explicit TCPConnection(Service& service,
                         std::unique_ptr<boost::asio::ip::tcp::socket> socket,
                         int connectionTry = 0);

  explicit TCPConnection(const TCPConnection& o, WeakStatePtr weakPtr)
    : TCPConnection(o) {
    // Reset the state store to use the weak variant instead.
    m_stateStore = weakPtr;
  }

  ~TCPConnection();

  void setResumeMode(ResumeMode mode);

  void send(parac_message&& message);
  void send(parac_file&& file);
  void send(MessageSendQueue::EndTag&& end);
  void send(parac_message& message);
  void send(parac_file& file);
  void send(MessageSendQueue::EndTag& end);
  void sendACK(uint32_t id, parac_status status);

  void conditionallyTriggerWriteHandler();

  private:
  struct InitiatorMessage;

  std::variant<WeakStatePtr, SharedStatePtr> m_stateStore;

  SharedStatePtr state() {
    if(m_stateStore.index() == 1) {
      return std::get<SharedStatePtr>(m_stateStore);
    } else {
      auto weak = std::get<WeakStatePtr>(m_stateStore);
      return weak.lock();
    }
  }
  SharedStatePtr state() const {
    if(m_stateStore.index() == 1) {
      auto& p = std::get<SharedStatePtr>(m_stateStore);
      assert(p);
      return p;
    } else {
      auto weak = std::get<WeakStatePtr>(m_stateStore);
      return weak.lock();
    }
  }

  void readHandler(boost::system::error_code ec, size_t bytes_received);
  void writeHandler(boost::system::error_code ec, size_t bytes_sent);

  bool shouldHandlerBeEnded();

  bool handleInitiatorMessage(const InitiatorMessage& init);
  bool handleReceivedACK(const PacketHeader& ack);
  bool handleReceivedMessage();
  void handleReceivedFileStart();
  void handleReceivedFileChunk();
  void handleReceivedFile();

  static void compute_node_free_func(parac_compute_node* n);
  static void compute_node_func(parac_compute_node* n);
  static void compute_node_send_message_to_func(parac_compute_node* n,
                                                parac_message* msg);
  static void compute_node_send_file_to_func(parac_compute_node* n,
                                             parac_file* file);

  template<typename S, typename B, typename CB>
  inline void async_write(S& socket, B&& buffer, CB& cb);

  template<typename S, typename B, typename CB>
  inline void async_read(S& socket, B&& buffer, CB cb, parac_id remoteId);

  void close();

  void setKeepaliveTimer();
};

constexpr const char*
ConnectionResumeModeToStr(TCPConnection::ResumeMode resumeMode) {
  switch(resumeMode) {
    case TCPConnection::ResumeMode::EndAfterShutdown:
      return "EndAfterShutdown";
    case TCPConnection::ResumeMode::RestartAfterShutdown:
      return "RestartAfterShutdown";
  }
  return "Unknown Resume Mode";
}

std::ostream&
operator<<(std::ostream& o, TCPConnection::ResumeMode resumeMode);
}
