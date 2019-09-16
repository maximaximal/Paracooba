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
CNF::CNF(int64_t originId, std::string_view dimacsFile)
  : m_originId(originId)
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
CNF::CNF(const CNF& o)
  : m_originId(o.m_originId)
  , m_dimacsFile(o.m_dimacsFile)
{}

CNF::~CNF() {}

void
CNF::send(boost::asio::ip::tcp::socket* socket,
          CNFTree::Path path,
          SendFinishedCB cb,
          bool first)
{
  SendDataStruct* data = &m_sendData[socket];

  if(first) {
    data->cb = cb;
    data->offset = 0;
  }

  if(path == 0) {
    if(first) {
      // Send the filename, so that the used compression algorithm can be known.
      // Also transfer the terminating \0.
      boost::filesystem::path p(m_dimacsFile);
      std::string dimacsBasename = p.filename().string();
      socket->write_some(
        boost::asio::buffer(reinterpret_cast<const char*>(&path), sizeof(path)));
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
                             std::bind(&CNF::sendCB, this, data, path, socket));
  } else {
    // Transmit all decisions leading up to this node.
  }
}

void
CNF::sendCB(SendDataStruct* data,
            CNFTree::Path path,
            boost::asio::ip::tcp::socket* socket)
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
    send(socket, path, data->cb, false);
  }
}

void
CNF::receive(boost::asio::ip::tcp::socket* socket,
             char* buf,
             std::size_t length)
{
  using namespace boost::filesystem;
  assert(socket);

  ReceiveDataStruct& d = m_receiveData[socket];

  while(length > 0 || buf == nullptr) {
    switch(d.state) {
      case ReceivePath:
        if(length < sizeof(ReceiveDataStruct::path)) {
          // Remote has directly aborted the transmission.
          m_receiveData.erase(socket);
          return;
        }
        d.path = *(reinterpret_cast<int64_t*>(buf));
        buf += sizeof(ReceiveDataStruct::path);
        length -= sizeof(ReceiveDataStruct::path);

        if(d.path == 0) {
          d.state = ReceiveFileName;
        } else {
          d.state = ReceiveCube;
        }
        break;
      case ReceiveFileName: {
        // First, receive the filename.
        bool end = false;
        while(length > 0 && !end) {
          if(*buf == '\0') {
            d.state = ReceiveFile;

            path dir =
              temp_directory_path() / ("paracuber-" + std::to_string(getpid()));
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
          m_receiveData.erase(socket);
          return;
        }

        m_ofstream.write(buf, length);
        length = 0;
        break;
      case ReceiveCube:
        for(std::size_t i = 0; i < length; i += sizeof(CNFTree::CubeVar)) {
          CNFTree::CubeVar* var = nullptr;
          if(d.cubeVarReceivePos == 0 && length >= sizeof(CNFTree::CubeVar)) {
            // Easy receive.
            var = reinterpret_cast<CNFTree::CubeVar*>(buf);
            buf += sizeof(CNFTree::CubeVar);
            length -= sizeof(CNFTree::CubeVar);
          } else {
            // Cube var split into multiple buffers, reconstructing.
            size_t l =
              (length >= sizeof(CNFTree::CubeVar) ? sizeof(CNFTree::CubeVar)
                                                  : length) -
              d.cubeVarReceivePos;

            std::copy(buf, buf + l, d.cubeVarReceiveBuf + d.cubeVarReceivePos);

            buf += l;
            length -= l;

            if(d.cubeVarReceivePos == sizeof(CNFTree::CubeVar)) {
              d.cubeVarReceivePos = 0;
              var = reinterpret_cast<CNFTree::CubeVar*>(d.cubeVarReceiveBuf);
            }
          }
          if(var) {
            // Valid literal received, insert into CNFTree.
          }
        }
        break;
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
