#ifndef PARACUBER_COMMUNICATOR_HPP
#define PARACUBER_COMMUNICATOR_HPP

#include "cluster-statistics.hpp"
#include "cnftree.hpp"
#include "log.hpp"
#include "readywaiter.hpp"

#include <any>
#include <functional>
#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/version.hpp>

namespace boost {
namespace asio {
#if(BOOST_VERSION / 100 % 1000) >= 69
class io_context;
using io_service = io_context;
class signal_set;
#else
class io_service;
#endif
}
namespace system {
class error_code;
}
}

using IOServicePtr = std::shared_ptr<boost::asio::io_service>;

namespace paracuber {

class Runner;
class CNF;
class NetworkedNode;

using RunnerPtr = std::shared_ptr<Runner>;
using ClusterStatisticsPtr = std::shared_ptr<ClusterStatistics>;

/** @brief Hub for all local & network communication processes between nodes.
 *
 * This node owns the boost asio io_service that is responsible for all timing
 * operations on the local node and for communicating with other nodes.
 *
 * \section SendAndReceiveFormulas Sending and Receiving Formulas
 *
 * A solver is not sent directly. Instead, only the changes to the formula state
 * are transmitted. This means, that the root formula is sent once and cubes are
 * sent as needed. More on synchronising cubes in @ref CNFTree.
 *
 * \dotfile solver-network-flow.dot
 */
class Communicator : public std::enable_shared_from_this<Communicator>
{
  public:
  class UDPServer;
  class TCPServer;
  class TCPClient;

  /** @brief Constructor */
  Communicator(ConfigPtr config, LogPtr log);
  /** @brief Destructor */
  virtual ~Communicator();

  /** @brief Runs the communicator thread and blocks until termination.
   *
   * This starts the communicator worker thread. It stops after all work is
   * completed.
   */
  void run();

  /** @brief Ends the communicator and stops all running services.
   */
  void exit();

  /** @brief Start the internal runner thread pool without blocking. */
  void startRunner();

  /** @brief Get the active \ref Runner class instance for running \ref Task
   * objects.
   */
  inline RunnerPtr getRunner() { return m_runner; }
  inline IOServicePtr getIOService() { return m_ioService; }
  inline ClusterStatisticsPtr getClusterStatistics()
  {
    return m_clusterStatistics;
  }

  inline int64_t getAndIncrementCurrentMessageId()
  {
    return m_currentMessageId++;
  }

  enum class TCPClientMode
  {
    TransmitCNF,
    TransmitAllowanceMap
  };

  void sendCNFToNode(std::shared_ptr<CNF> cnf,
                     CNFTree::Path path,
                     NetworkedNode* nn);

  void sendAllowanceMapToNodeWhenReady(std::shared_ptr<CNF> cnf,
                                       NetworkedNode* nn);

  private:
  ConfigPtr m_config;
  LogPtr m_log;
  IOServicePtr m_ioService;
  std::any m_ioServiceWork;
  Logger m_logger;
  std::unique_ptr<boost::asio::signal_set> m_signalSet;
  RunnerPtr m_runner;
  ClusterStatisticsPtr m_clusterStatistics;

  int64_t m_currentMessageId = INT64_MIN;

  void signalHandler(const boost::system::error_code& error, int signalNumber);

  // Listeners
  void listenForIncomingUDP(uint16_t port);
  void listenForIncomingTCP(uint16_t port);

  std::unique_ptr<UDPServer> m_udpServer;
  std::unique_ptr<TCPServer> m_tcpServer;

  // Tasks
  void task_announce(NetworkedNode* nn = nullptr);
  void task_requestAnnounce(int64_t id = 0,
                            std::string regex = "",
                            NetworkedNode* nn = nullptr);
  void task_offlineAnnouncement(NetworkedNode* nn = nullptr);

  /** @brief Ticks are called every 100ms.
   *
   * The main task of ticks is to send out statistics updates to all other
   * connected nodes.
   */
  void tick();
  boost::asio::steady_timer m_tickTimer;
};

std::ostream&
operator<<(std::ostream& o, Communicator::TCPClientMode mode);

using CommunicatorPtr = std::shared_ptr<Communicator>;
}

#endif
