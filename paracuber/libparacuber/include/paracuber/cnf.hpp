#ifndef PARACUBER_CNF_HPP
#define PARACUBER_CNF_HPP

#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/ip/tcp.hpp>

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
  /** @brief Maximum depth means 64, which is too high.
   *
   * This encodes a CNF which is not yet assigned.
   */
  static const uint64_t UNINITIALIZED_CNF_PREV = 0b00111111u;

  using CubeVarType = int32_t;
  using CubeVector = std::vector<CubeVarType>;

  /** @brief Construct a CNF from existing literals based on DIMACS file or on
   * previous CNF.
   */
  CNF(int64_t originId,
      uint64_t previous = UNINITIALIZED_CNF_PREV,
      std::string_view dimacsFile = "");
  /** @brief Constructs a CNF using a variable count for then applying a cube to
   * it.
   *
   * The variable count is reserved internally.
   */
  CNF(int64_t originId, uint64_t previous, size_t varCount);

  /** @brief Copy another CNF formula.
   */
  CNF(const CNF& o);
  virtual ~CNF();

  uint64_t getPrevious() { return m_previous; }
  std::string_view getDimacsFile() { return m_dimacsFile; }

  using SendFinishedCB = std::function<void()>;

  void send(boost::asio::ip::tcp::socket* socket,
            SendFinishedCB finishedCallback,
            bool first = true);
  void receive(char* buf, std::size_t length);

  inline uint64_t getPath()
  {
    return m_previous & (0xFFFFFFFFFFFFFFFFu & 0b11000000u);
  }
  inline uint8_t getDepth() { return m_previous & 0b00111111u; }

  /** @brief Get a writeable reference to the internal cube vector. */
  inline CubeVector& getCube() { return m_cubeVector; }

  inline int64_t getOriginId() { return m_originId; }

  inline void setPath(uint64_t p)
  {
    m_previous |=
      (m_previous & 0b00111111) | (p & ~(0xFFFFFFFFFFFFFFFFu & 0b11000000u));
  }

  inline void setDepth(uint8_t d) { m_previous = getPath() | d; }

  inline bool isRootCNF() { return m_previous == 0; }

  void setRootTask(std::unique_ptr<CaDiCaLTask> root);
  CaDiCaLTask* getRootTask();

  private:
  struct SendDataStruct
  {
    off_t offset = 0;
    SendFinishedCB cb;
  };
  enum ReceiveState
  {
    ReceiveFileName,
    ReceiveFile,
  };

  void sendCB(SendDataStruct* data, boost::asio::ip::tcp::socket* socket);

  int64_t m_originId = 0;
  uint64_t m_previous = -1;
  std::string m_dimacsFile = "";
  ReceiveState m_receiveState = ReceiveFileName;

  // Ofstream for outputting the original CNF file.
  std::ofstream m_ofstream;

  int m_fd = 0;
  size_t m_fileSize = 0;

  std::map<boost::asio::ip::tcp::socket*, SendDataStruct> m_sendData;
  CubeVector m_cubeVector;
  std::unique_ptr<CaDiCaLTask> m_rootTask;
};
}

#endif
