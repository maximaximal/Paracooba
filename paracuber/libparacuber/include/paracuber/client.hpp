#ifndef PARACUBER_CLIENT_HPP
#define PARACUBER_CLIENT_HPP

#include <memory>
#include <string_view>

#include "log.hpp"
#include "taskresult.hpp"

namespace paracuber {
class Config;
class Communicator;

/** @brief Main interaction point with the solver as a user.
 */
class Client
{
  public:
  /** @brief Constructor
   */
  Client(ConfigPtr config,
         LogPtr log,
         std::shared_ptr<Communicator> communicator);
  /** @brief Destructor.
   */
  ~Client();

  /** @brief Read DIMACS file into the internal solver instance, get path from
   * config. */
  std::string_view getDIMACSSourcePathFromConfig();

  /** @brief Try to solve the current formula.
   *
   * @return status code,
   *         - TaskResult::Status::Unsolved
   *         - TaskResult::Status::Satisfiable
   *         - TaskResult::Status::Unsatisfiable
   */
  TaskResult::Status solve();

  private:
  ConfigPtr m_config;
  std::shared_ptr<Communicator> m_communicator;

  Logger m_logger;
};
}

#endif
