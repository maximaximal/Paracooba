#pragma once

#include <functional>
#include <string>

#include <boost/asio/ip/tcp.hpp>

namespace parac::communicator {
class FileSender {
  public:
  using FinishedCB = std::function<void()>;

  FileSender(const std::string& source_file,
             boost::asio::ip::tcp::socket& socket,
             FinishedCB cb);

  ~FileSender() = default;

  void send() { send_chunk(); }

  static size_t file_size(const std::string &file);

  private:
  struct State {
    State(boost::asio::ip::tcp::socket& s)
      : target_socket(s) {}
    std::string source_file;
    int fd = 0;
    boost::asio::ip::tcp::socket& target_socket;
    size_t file_size;
    off_t offset;
    FinishedCB cb;
  };
  std::shared_ptr<State> m_state;

  void send_chunk(bool first = true);
};
}
