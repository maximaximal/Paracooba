#ifndef PARACUBER_COMMUNICATOR_HPP
#define PARACUBER_COMMUNICATOR_HPP

#include "log.hpp"
#include <any>
#include <memory>

#include <boost/version.hpp>
#include <boost/asio/signal_set.hpp>

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

/** @brief Hub for all local & network communication processes between nodes.
 *
 * This node owns the boost asio io_service that is responsible for all timing
 * operations on the local node and for communicating with other nodes.
 *
 * \section SendReceiveSolverInstances Sending and Receiving Solver Instances
 *
 * To send solver instances, a new TCP stream is opened. A solver gets
 * serialised into a buffer and directly transmitted over the stream. It then
 * gets reconstructed and the stream gets closed.
 *
 * \dotfile solver-network-flow.dot
 */
class Communicator : public std::enable_shared_from_this<Communicator>
{
  public:
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

  /** @brief Get the active \ref Runner class instance for running \ref Task
   * objects.
   */
  inline std::shared_ptr<Runner> getRunner() { return m_runner; }

  private:
  ConfigPtr m_config;
  IOServicePtr m_ioService;
  std::any m_ioServiceWork;
  Logger m_logger;
  std::unique_ptr<boost::asio::signal_set> m_signalSet;
  std::shared_ptr<Runner> m_runner;

  void signalHandler(const boost::system::error_code& error, int signalNumber);
};

using CommunicatorPtr = std::shared_ptr<Communicator>;
}

#endif
