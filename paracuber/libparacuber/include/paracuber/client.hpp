#ifndef PARACUBER_CLIENT_HPP
#define PARACUBER_CLIENT_HPP

#include <memory>
#include <string_view>

#include "log.hpp"

namespace CaDiCaL {
class Solver;
}

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
  const char* readDIMACSFromConfig();

  /** @brief Read DIMACS file into the internal solver instance. */
  const char* readDIMACS(std::string_view sourcePath);

  /** @brief Try to solve the current formula.
   *
   * @return status code,
   *         - 0 UNSOLVED
   *         - 10 SATISFIED
   *         - 20 UNSATISFIABLE
   */
  int solve();

  private:
  ConfigPtr m_config;
  std::shared_ptr<Communicator> m_communicator;

  std::shared_ptr<CaDiCaL::Solver> m_solver;

  Logger m_logger;
};
}

#endif
