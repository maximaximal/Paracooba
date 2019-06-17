#ifndef PARACUBER_DAEMON_HPP
#define PARACUBER_DAEMON_HPP

#include <memory>

namespace paracuber {
class Config;
/** @brief Daemonised solver mode that waits for tasks and sends and receives
 * statistics to/from other solvers.
 */
class Daemon
{
  public:
  /** @brief Constructor
   */
  Daemon(std::shared_ptr<Config> config);
  /** @brief Destructor
   */
  ~Daemon();

  private:
  std::shared_ptr<Config> m_config;
};
}

#endif
