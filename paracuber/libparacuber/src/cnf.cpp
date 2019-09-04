#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/cadical_task.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <iostream>

extern "C"
{
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace paracuber {
CNF::CNF(int64_t originId, uint64_t previous, std::string_view dimacsFile)
  : m_originId(originId)
  , m_previous(previous)
  , m_dimacsFile(dimacsFile)
{
  if(dimacsFile != "") {
    struct stat statbuf;
    int result = stat(m_dimacsFile.c_str(), &statbuf);
    assert(result != -1);

    m_fileSize = statbuf.st_size;
    m_fd = open(m_dimacsFile.c_str(), O_RDONLY);
    assert(m_fd > 0);
  }
}
CNF::CNF(int64_t originId, uint64_t previous, size_t varCount)
  : m_originId(originId)
  , m_previous(previous)
  , m_cubeVector(varCount)
  , m_dimacsFile("")
{}
CNF::CNF(const CNF& o)
  : m_originId(o.m_originId)
  , m_previous(o.m_previous)
  , m_cubeVector(o.m_cubeVector.size())
  , m_dimacsFile(o.m_dimacsFile)
{}

CNF::~CNF() {}

void
CNF::send(boost::asio::ip::tcp::socket* socket, SendFinishedCB cb, bool first)
{
  SendDataStruct* data = &m_sendData[socket];

  if(first) {
    data->cb = cb;
    data->offset = 0;
  }

  if(m_previous == 0) {
    if(first) {
      // Send the filename, so that the used compression algorithm can be known.
      // Also transfer the terminating \0.
      boost::filesystem::path p(m_dimacsFile);
      std::string dimacsBasename = p.filename().string();
      socket->write_some(
        boost::asio::buffer(dimacsBasename.c_str(), dimacsBasename.size() + 1));
    }

    // This is the root CNF, send over the file directly using the sendfile()
    // syscall.
    int ret =
      sendfile(socket->native_handle(), m_fd, &data->offset, m_fileSize);
    if(ret == -1) {
      std::cerr << "ERROR DURING SENDFILE: " << strerror(errno) << std::endl;
      m_sendData.erase(socket);
    }

    socket->async_write_some(boost::asio::null_buffers(),
                             std::bind(&CNF::sendCB, this, data, socket));
  } else {
    // This is a cube, so only the cube must be transmitted. A cube always
    // resides in memory, so this transfer can be done directly in one shot.
    socket->async_write_some(
      boost::asio::buffer(reinterpret_cast<char*>(m_cubeVector.data()),
                          m_cubeVector.size() * sizeof(CubeVarType)),
      std::bind([this, data, socket]() {
        data->cb();
        m_sendData.erase(socket);
      }));
  }
}

void
CNF::sendCB(SendDataStruct* data, boost::asio::ip::tcp::socket* socket)
{
  if(data->offset >= m_fileSize) {
    data->cb();
    // This not only erases the send data structure,
    // but also frees the last reference to the
    // shared_ptr of the TCPClient instance that was
    // calling this function. The connection is then
    // closed.
    m_sendData.erase(socket);
  } else {
    // Repeated sends, as long as the file is transferred.
    send(socket, data->cb, false);
  }
}

void
CNF::receive(char* buf, std::size_t length)
{
  if(m_previous == 0) {
    using namespace boost::filesystem;

    // Receiving the root formula!
    while(length > 0 || buf == nullptr) {
      switch(m_receiveState) {
        case ReceiveFileName: {
          // First, receive the filename.
          bool end = false;
          while(length > 0 && !end) {
            if(*buf == '\0') {
              m_receiveState = ReceiveFile;

              path dir = temp_directory_path() /
                         ("paracuber-" + std::to_string(getpid()));
              path p = dir / unique_path();
              p += "-" + m_dimacsFile;

              m_dimacsFile = p.string();

              if(!exists(dir)) {
                create_directory(dir);
              }

              m_ofstream.open(m_dimacsFile, std::ios::out);
              end = true;
            } else {
              m_dimacsFile += *buf;
            }
            ++buf;
            --length;
          }
          break;
        }
        case ReceiveFile:
          if(length == 0) {
            // This marks the end of the transmission, the file is finished.
            m_ofstream.close();
            return;
          }

          m_ofstream.write(buf, length);
          length = 0;
          break;
      }
    }
  } else {
    // Receiving a cube! This always works, also with subsequent sends.

    // If this condition breaks, this will not work anymore and needs fixing!
    assert(length % sizeof(CubeVarType));

    for(std::size_t i = 0; i < length; i += sizeof(CubeVarType)) {
      m_cubeVector.push_back(*reinterpret_cast<CubeVarType*>(buf + i));
    }
  }
}

void
CNF::setRootTask(std::unique_ptr<CaDiCaLTask> root)
{
  m_rootTask = std::move(root);
}
CaDiCaLTask*
CNF::getRootTask()
{
  return m_rootTask.get();
}

}
