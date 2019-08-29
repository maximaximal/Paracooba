#include "../include/paracuber/cnf.hpp"

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

CNF::~CNF() {}

void
CNF::send(boost::asio::ip::tcp::socket* socket, SendFinishedCB cb)
{
  SendDataStruct* data = &m_sendData[socket];
  data->cb = cb;

  if(m_previous == 0) {
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
    send(socket, data->cb);
  }
}

void
CNF::receive(char* buf, std::size_t length)
{
  if(m_previous == 0) {
    // Receiving the root formula!
    using namespace boost::filesystem;
    if(m_dimacsFile == "") {
      path dir =
        temp_directory_path() / ("paracuber-" + std::to_string(getpid()));
      path p = dir / unique_path();
      m_dimacsFile = p.string();

      if(!exists(dir)) {
        create_directory(dir);
      }

      m_ofstream.open(m_dimacsFile, std::ios::out);
    }
    if(length == 0) {
      // This marks the end of the transmission, the file is finished.
      m_ofstream.close();
      return;
    }

    m_ofstream.write(buf, length);
  } else {
    // Receiving a cube!
    for(std::size_t i = 0; i < length; i += sizeof(CubeVarType)) {
      m_cubeVector.push_back(*reinterpret_cast<CubeVarType*>(buf + i));
    }
  }
}
}
