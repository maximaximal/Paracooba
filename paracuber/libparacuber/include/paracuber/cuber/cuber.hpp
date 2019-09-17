#ifndef PARACUBER_CUBER_CUBER_HPP
#define PARACUBER_CUBER_CUBER_HPP

#include "../cnftree.hpp"
#include "../log.hpp"
#include <memory>

namespace paracuber {
class CNF;

namespace cuber {

/** @brief Base class for all cubing algorithms.
 *
 * This class is initialised once per formula (daemon and client), so the
 * algorithm can track internal statistics to improve cube selection if desired.
 */
class Cuber
{
  public:
  explicit Cuber(ConfigPtr config, LogPtr log, CNF &rootCNF);
  virtual ~Cuber();

  /** @brief Generates the next cube on a given path.
   *
   * Required method for implementing cubing algorithms.
   *
   * @return The cube var.
   */
  virtual CNFTree::CubeVar generateCube(CNFTree::Path path) = 0;

  protected:
  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  CNF &m_rootCNF;

  private:
};
}
}

#endif
