#ifndef PARACUBER_CNF_HPP
#define PARACUBER_CNF_HPP

#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <string_view>

#include <boost/asio/ip/tcp.hpp>

namespace paracuber {
class NetworkedNode;

/** @brief This class represents a CNF formula that can be transferred directly.
 *
 * A dimacs file can be sent, if it is the root formula. All appended cubes
 * are transferred only afterwards, the root formula is not touched again.
 */
class CNF
{
  public:
  /** @brief Construct a CNF from existing literals based on DIMACS file or on
   * previous CNF.
   */
  CNF(int64_t previous = -1, std::string_view dimacsFile = "");
  virtual ~CNF();

  int64_t getPrevious() { return m_previous; }
  std::string_view getDimacsFile() { return m_dimacsFile; }

  using SendFinishedCB = std::function<void()>;

  void send(boost::asio::ip::tcp::socket* socket,
            SendFinishedCB finishedCallback);
  void receiveFile(char* buf, std::size_t length);

  private:
  struct SendDataStruct
  {
    off_t offset = 0;
    SendFinishedCB cb;
  };

  void sendCB(SendDataStruct* data, boost::asio::ip::tcp::socket* socket);

  int64_t m_previous = -1;
  std::string m_dimacsFile = "";

  // Ofstream for outputting the original CNF file.
  std::ofstream m_ofstream;

  int m_fd = 0;
  size_t m_fileSize = 0;

  std::map<boost::asio::ip::tcp::socket*, SendDataStruct> m_sendData;
};
}

#endif
