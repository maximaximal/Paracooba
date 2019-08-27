#include "../include/paracuber/cnf.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace paracuber {
CNF::CNF(int64_t previous, std::string_view dimacsFile)
  : m_previous(previous)
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

CNF::~CNF() {}

void
CNF::send(boost::asio::ip::tcp::socket* socket, SendFinishedCB cb)
{
  SendDataStruct* data = &m_sendData[socket];
  data->cb = cb;

  if(m_previous == 0) {
    // This is the root CNF, send over the file directly using the sendfile()
    // syscall.
    sendfile(socket->native_handle(), m_fd, &data->offset, m_fileSize);

    socket->async_write_some(boost::asio::null_buffers(),
                             std::bind([data]() { data->cb(); }));
  }
}

void
CNF::receiveFile(char* buf, std::size_t length)
{
  using namespace boost::filesystem;
  if(m_dimacsFile == "") {
    path p = temp_directory_path() / unique_path();
    m_dimacsFile = p.string();
    m_ofstream.open(m_dimacsFile);
  }
  if(length == 0) {
    // This marks the end of the transmission, the file is finished.
    m_ofstream.close();
    return;
  }

  m_ofstream.write(buf, length);
}
}
