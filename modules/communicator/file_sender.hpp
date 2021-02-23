#pragma once

#include <boost/system/error_code.hpp>
#include <functional>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include <boost/asio/steady_timer.hpp>

#include "packet.hpp"

namespace parac::communicator {
class Service;

class FileSender {
  public:
  using FinishedCB = std::function<void(const boost::system::error_code&)>;

  FileSender() = default;
  FileSender(const std::string& source_file,
             boost::asio::ip::tcp::socket& socket,
             FinishedCB cb,
             Service& service);

  ~FileSender() = default;

  void send();

  static size_t file_size(const std::string& file);

  private:
  struct State {
    explicit State(boost::asio::ip::tcp::socket& s, Service& service);
    std::string source_file;
    int fd = 0;
    boost::asio::ip::tcp::socket& target_socket;
    off_t file_size;
    off_t offset = 0;
    FinishedCB cb;
    std::string fileHeaderFilename;
    PacketFileHeader fileHeader;
    class Service& service;
    boost::asio::steady_timer eagainTimer;
    int eagainTries = 0;
  };
  std::shared_ptr<State> m_state;

  void send_chunk(bool first = true);
};
}
