#ifndef PARACUBER_COMMUNICATOR_HPP
#define PARACUBER_COMMUNICATOR_HPP

#include "log.hpp"
#include <any>
#include <memory>

namespace boost {
namespace asio {
class io_context;
using io_service = io_context;
}
}

using IOServicePtr = std::shared_ptr<boost::asio::io_service>;

namespace paracuber {
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
class Communicator
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

  private:
  IOServicePtr m_ioService;
  std::any m_ioServiceWork;
  ConfigPtr m_config;
  Logger m_logger;
};

using CommunicatorPtr = std::shared_ptr<Communicator>;
}

#endif
