#ifndef PARACUBER_DAEMON_HPP
#define PARACUBER_DAEMON_HPP

#include "log.hpp"
#include <memory>

namespace paracuber {
class Communicator;
/** @brief Daemonised solver mode that waits for tasks and sends and receives
 * statistics to/from other solvers.
 */
class Daemon
{
  public:
  /** @brief Constructor
   */
  Daemon(ConfigPtr config,
         LogPtr log,
         std::shared_ptr<Communicator> communicator);
  /** @brief Destructor
   */
  ~Daemon();

  private:
  std::shared_ptr<Config> m_config;
};
}

#endif
