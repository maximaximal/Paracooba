#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>

#include <boost/asio/ip/tcp.hpp>

struct parac_message;
struct parac_file;

namespace boost::system {
class error_code;
}

namespace parac::communicator {
class Service;

/** @brief Class representing a connection between two compute nodes.
 *
 * This class handles the complete connection live-cycle. Connections are
 * established over TCP sockets. If a socket is ended, it is tried to reconnect
 * to the remote host.
 */
class TCPConnection {
  public:
  enum Lifecycle { Initializing, Active, Dead };
  enum TransmitMode {
    TransmitInit,
    TransmitMessage,
    TransmitFile,
    TransmitEnd
  };
  enum ResumeMode { EndAfterShutdown, RestartAfterShutdown };
  struct EndTag;

  /** @brief Initialize a paracooba connection on an opened socket.
   */
  explicit TCPConnection(Service& service,
                         boost::asio::ip::tcp::socket&& socket,
                         int connectionTry = 0);
  ~TCPConnection();

  void send(parac_message& message);
  void send(parac_file& file);
  void send(EndTag& end);
  void send(parac_message&& message);
  void send(parac_file&& file);
  void send(EndTag&& end);

  private:
  struct SendQueueEntry;
  struct State;
  std::shared_ptr<State> m_state;

  void send(SendQueueEntry&& e);

  void readHandler(boost::system::error_code ec, size_t bytes_received);
  void writeHandler(boost::system::error_code ec, size_t bytes_sent);
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
