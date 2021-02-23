#include "file_sender.hpp"
#include "packet.hpp"
#include <boost/asio/error.hpp>
#include <boost/filesystem/path.hpp>
#include <cassert>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
}

#include "service.hpp"

#include <paracooba/common/log.h>

namespace parac::communicator {
FileSender::FileSender(const std::string& source_file,
                       boost::asio::ip::tcp::socket& socket,
                       FinishedCB cb,
                       Service& service)
  : m_state(std::make_shared<State>(socket, service)) {
  m_state->source_file = source_file;
  m_state->cb = cb;

  {
    boost::filesystem::path p(m_state->source_file);

    assert(p.has_filename());
    m_state->fileHeaderFilename = p.filename().string();

    if(m_state->fileHeaderFilename.length() > 255) {
      size_t start;
      start = m_state->fileHeaderFilename.length() - 255;
      m_state->fileHeaderFilename =
        m_state->fileHeaderFilename.substr(start, std::string::npos);

      parac_log(PARAC_COMMUNICATOR,
                PARAC_LOCALWARNING,
                "Had to compact filename for sending! Compacted {} to {}.",
                source_file,
                m_state->fileHeaderFilename);
    }

    strcpy(m_state->fileHeader.name, m_state->fileHeaderFilename.c_str());
  }

  assert(socket.is_open());

  m_state->file_size = file_size(source_file);
  if(m_state->file_size == 0) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Could not find file {}!",
              source_file);
    return;
  }

  m_state->fd = open(source_file.c_str(), O_RDONLY);

  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Sending file {} with {} bytes from endpoint "
            "{} to endpoint {}.",
            source_file,
            m_state->file_size,
            socket.local_endpoint(),
            socket.remote_endpoint());

  assert(m_state->fd);
}

size_t
FileSender::file_size(const std::string& p) {
  struct stat statbuf;
  int result = stat(p.c_str(), &statbuf);
  if(result == -1) {
    return 0;
  }
  return statbuf.st_size;
}

#define BUF(SOURCE) boost::asio::buffer(&SOURCE, sizeof(SOURCE))

void
FileSender::send() {
  m_state->target_socket.async_write_some(
    BUF(m_state->fileHeader), std::bind(&FileSender::send_chunk, *this, true));
}

#undef BUF

void
FileSender::send_chunk(bool first) {
  if(!first && m_state->offset >= m_state->file_size) {
    m_state->cb(boost::system::error_code());
    return;
  }

  auto sc = std::bind(&FileSender::send_chunk, *this, false);

  int ret = sendfile(m_state->target_socket.native_handle(),
                     m_state->fd,
                     &m_state->offset,
                     m_state->file_size);

  if(ret == -1) {
    if(errno == EAGAIN) {
      if(++m_state->eagainTries > 3) {
        parac_log(PARAC_COMMUNICATOR,
                  PARAC_LOCALERROR,
                  "Got EAGAIN for the {}th time during sendfile to endpoint "
                  "{}! Exiting connection with error.",
                  m_state->eagainTries,
                  m_state->target_socket.remote_endpoint());
        return m_state->cb(boost::asio::error::basic_errors::shut_down);
      } else {
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_LOCALWARNING,
          "Got EAGAIN during sendfile to endpoint {}! Waiting for 500ms "
          "(network timeout ms) and trying again (try {}).",
          m_state->target_socket.remote_endpoint(),
          m_state->eagainTries);
        m_state->eagainTimer.expires_from_now(std::chrono::milliseconds(500));
        return m_state->eagainTimer.async_wait(sc);
      }
    } else {
      parac_log(
        PARAC_COMMUNICATOR,
        PARAC_LOCALERROR,
        "Unknown error during sendfile! Error: {}. Giving error to connection.",
        std::strerror(errno));
      m_state->cb(boost::asio::error::basic_errors::shut_down);
      return;
    }
  }

  m_state->eagainTries = 0;

  m_state->target_socket.async_write_some(boost::asio::null_buffers(), sc);
}

FileSender::State::State(boost::asio::ip::tcp::socket& s, Service& service)
  : target_socket(s)
  , service(service)
  , eagainTimer(service.ioContext()) {}
}
