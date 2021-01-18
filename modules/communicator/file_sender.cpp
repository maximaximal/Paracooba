#include "file_sender.hpp"
#include "packet.hpp"
#include <boost/filesystem/path.hpp>
#include <cassert>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
}

#include <paracooba/common/log.h>

namespace parac::communicator {
FileSender::FileSender(const std::string& source_file,
                       boost::asio::ip::tcp::socket& socket,
                       FinishedCB cb)
  : m_state(std::make_shared<State>(socket)) {
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
    m_state->cb();
    return;
  }

  int ret = sendfile(m_state->target_socket.native_handle(),
                     m_state->fd,
                     &m_state->offset,
                     m_state->file_size);

  if(ret == -1) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Error during sendfile! Error: {}",
              std::strerror(errno));
    return;
  }

  m_state->target_socket.async_write_some(
    boost::asio::null_buffers(),
    std::bind(&FileSender::send_chunk, *this, false));
}
}
