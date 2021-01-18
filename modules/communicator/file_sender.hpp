#pragma once

#include <functional>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include "packet.hpp"

namespace parac::communicator {
class FileSender {
  public:
  using FinishedCB = std::function<void()>;

  FileSender(const std::string& source_file,
             boost::asio::ip::tcp::socket& socket,
             FinishedCB cb);

  ~FileSender() = default;

  void send();

  static size_t file_size(const std::string& file);

  private:
  struct State {
    explicit State(boost::asio::ip::tcp::socket& s)
      : target_socket(s) {}
    std::string source_file;
    int fd = 0;
    boost::asio::ip::tcp::socket& target_socket;
    off_t file_size;
    off_t offset = 0;
    FinishedCB cb;
    std::string fileHeaderFilename;
    PacketFileHeader fileHeader;
  };
  std::shared_ptr<State> m_state;

  void send_chunk(bool first = true);
};
}
