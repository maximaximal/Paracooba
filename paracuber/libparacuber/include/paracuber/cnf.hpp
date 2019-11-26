#ifndef PARACUBER_CNF_HPP
#define PARACUBER_CNF_HPP

#include <boost/signals2/signal.hpp>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/ip/tcp.hpp>

#include "cnftree.hpp"
#include "log.hpp"
#include "readywaiter.hpp"
#include "taskresult.hpp"

namespace paracuber {
class NetworkedNode;
class CaDiCaLTask;
class TaskFactory;

namespace cuber {
class Registry;
}

/** @brief This class represents a CNF formula that can be transferred directly.
 *
 * A dimacs file can be sent, if it is the root formula. All appended cubes
 * are transferred only afterwards, the root formula is not touched again.
 */
class CNF : public std::enable_shared_from_this<CNF>
{
  public:
  /** @brief Construct a CNF from existing literals based on DIMACS file or on
   * previous CNF.
   */
  CNF(ConfigPtr config,
      LogPtr log,
      int64_t originId,
      std::string_view dimacsFile = "");

  /** @brief Copy another CNF formula.
   */
  CNF(const CNF& o);
  virtual ~CNF();

  enum TransmissionSubject
  {
    TransmitFormula = 1,
    TransmitAllowanceMap = 2,
    TransmitResult = 3,
    TransmissionSubjectUnknown = 4,
  };

  enum ReceiveState
  {
    ReceiveTransmissionSubject,
    ReceivePath,
    ReceiveFileName,
    ReceiveFile,
    ReceiveCube,
    ReceiveAllowanceMapSize,
    ReceiveAllowanceMap,
    ReceiveResultPath,
    ReceiveResultState,
    ReceiveResultSize,
    ReceiveResultData,
  };

  struct Result
  {
    CNFTree::Path p;
    CNFTree::State state;
    uint32_t size = 0;
    bool finished = false;
    std::shared_ptr<std::vector<uint8_t>> assignment =
      std::make_shared<std::vector<uint8_t>>();
    std::shared_ptr<CaDiCaLTask> task;
  };

  struct ReceiveDataStruct
  {
    TransmissionSubject subject = TransmissionSubjectUnknown;
    ReceiveState state = ReceiveTransmissionSubject;
    CNFTree::Path path = 0;
    uint8_t currentDepth = 0;
    size_t receiveVarPos = 0;
    char receiveVarBuf[8];
    int64_t originator = 0;
    Result* result = nullptr;
  };

  std::string_view getDimacsFile() { return m_dimacsFile; }

  using SendFinishedCB = std::function<void()>;
  using ResultFoundSignal = boost::signals2::signal<void(Result*)>;

  void send(boost::asio::ip::tcp::socket* socket,
            CNFTree::Path path,
            SendFinishedCB finishedCallback,
            bool first = true);
  void sendAllowanceMap(boost::asio::ip::tcp::socket* socket,
                        SendFinishedCB finishedCallback);
  void sendResult(boost::asio::ip::tcp::socket* socket,
                  CNFTree::Path p,
                  SendFinishedCB finishedCallback);

  TransmissionSubject receive(boost::asio::ip::tcp::socket* socket,
                              char* buf,
                              std::size_t length);

  inline int64_t getOriginId() { return m_originId; }

  void setRootTask(std::unique_ptr<CaDiCaLTask> root);
  CaDiCaLTask* getRootTask();

  cuber::Registry& getCuberRegistry() { return *m_cuberRegistry; }

  ReadyWaiter<CaDiCaLTask> rootTaskReady;

  bool readyToBeStarted() const;

  CNFTree& getCNFTree() { return *m_cnfTree; }

  void requestInfoGlobally(CNFTree::Path p, int64_t handle = 0);

  void setTaskFactory(TaskFactory* f) { m_taskFactory = f; }

  void solverFinishedSlot(const TaskResult& result, CNFTree::Path path);

  void handleFinishedResultReceived(Result& result);

  ResultFoundSignal& getResultFoundSignal() { return m_resultSignal; }

  void insertResult(CNFTree::Path p,
                    CNFTree::State state,
                    CNFTree::Path source);

  private:
  struct SendDataStruct
  {
    off_t offset = 0;
    SendFinishedCB cb;
    std::vector<CNFTree::CubeVar> decisions;
  };

  void connectToCNFTreeSignal();

  void sendCB(SendDataStruct* data,
              CNFTree::Path path,
              boost::asio::ip::tcp::socket* socket);

  int64_t m_originId = 0;
  std::string m_dimacsFile = "";

  // Ofstream for outputting the original CNF file.
  std::ofstream m_ofstream;

  int m_fd = 0;
  size_t m_fileSize = 0;

  std::map<boost::asio::ip::tcp::socket*, SendDataStruct> m_sendData;
  std::map<boost::asio::ip::tcp::socket*, ReceiveDataStruct> m_receiveData;

  using CaDiCaLTaskPtr = std::unique_ptr<CaDiCaLTask>;
  CaDiCaLTaskPtr m_rootTask;

  std::unique_ptr<cuber::Registry> m_cuberRegistry;
  std::unique_ptr<CNFTree> m_cnfTree;
  std::shared_mutex m_cnfTreeMutex;

  std::map<CNFTree::Path, Result> m_results;

  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  TaskFactory* m_taskFactory = nullptr;
  ResultFoundSignal m_resultSignal;
};

std::ostream&
operator<<(std::ostream& m, CNF::TransmissionSubject s);
std::ostream&
operator<<(std::ostream& m, CNF::ReceiveState s);
}

#endif
