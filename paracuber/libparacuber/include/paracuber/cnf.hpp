#ifndef PARACUBER_CNF_HPP
#define PARACUBER_CNF_HPP

#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/ip/tcp.hpp>

#include "log.hpp"
#include "cnftree.hpp"

namespace paracuber {
class NetworkedNode;
class CaDiCaLTask;

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
  CNF(LogPtr log, int64_t originId, std::string_view dimacsFile = "");

  /** @brief Copy another CNF formula.
   */
  CNF(const CNF& o);
  virtual ~CNF();

  std::string_view getDimacsFile() { return m_dimacsFile; }

  using SendFinishedCB = std::function<void()>;

  void send(boost::asio::ip::tcp::socket* socket,
            CNFTree::Path path,
            SendFinishedCB finishedCallback,
            bool first = true);
  void receive(boost::asio::ip::tcp::socket* socket,
               char* buf,
               std::size_t length);

  inline int64_t getOriginId() { return m_originId; }

  void setRootTask(std::unique_ptr<CaDiCaLTask> root);
  CaDiCaLTask* getRootTask();

  private:
  enum ReceiveState
  {
    ReceivePath,
    ReceiveFileName,
    ReceiveFile,
    ReceiveCube,
  };

  struct SendDataStruct
  {
    off_t offset = 0;
    SendFinishedCB cb;
  };
  struct ReceiveDataStruct
  {
    ReceiveState state = ReceivePath;
    CNFTree::Path path = 0;
    uint8_t currentDepth = 1;
    size_t cubeVarReceivePos = 0;
    char cubeVarReceiveBuf[sizeof(CNFTree::CubeVar)];
  };

  void sendCB(SendDataStruct* data, CNFTree::Path path, boost::asio::ip::tcp::socket* socket);

  int64_t m_originId = 0;
  std::string m_dimacsFile = "";

  // Ofstream for outputting the original CNF file.
  std::ofstream m_ofstream;

  int m_fd = 0;
  size_t m_fileSize = 0;

  std::map<boost::asio::ip::tcp::socket*, SendDataStruct> m_sendData;
  std::map<boost::asio::ip::tcp::socket*, ReceiveDataStruct> m_receiveData;

  std::unique_ptr<CaDiCaLTask> m_rootTask;
  CNFTree m_cnfTree;

  LogPtr m_log;
  Logger m_logger;
};
}

#endif
