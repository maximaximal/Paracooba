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
  explicit Cuber(ConfigPtr config, LogPtr log, CNF& rootCNF);
  virtual ~Cuber();

  using LiteralMap = std::vector<CNFTree::CubeVar>;

  /** @brief Generates the next cube on a given path.
   *
   * Required method for implementing cubing algorithms.
   *
   * @param path The path to the current position to be decided. This position
   * must be a leaf node.
   * @param var A reference to a decision variable that is going to be
   * overwritten in this function.
   * @return True if cubing was successful, false if no other cube should be
   * done.
   */
  virtual bool generateCube(CNFTree::Path path, CNFTree::CubeVar& var) = 0;

  static inline uint64_t getModuloComponent(CNFTree::Path p)
  {
    return (1 << (CNFTree::getDepth(p) - 1));
  }
  static inline uint64_t getAdditionComponent(CNFTree::Path p)
  {
    return CNFTree::getDepthShiftedPath(p);
  }

  protected:
  friend class Registry;

  ConfigPtr m_config;
  LogPtr m_log;
  Logger m_logger;
  CNF& m_rootCNF;

  LiteralMap* m_allowanceMap = nullptr;

  private:
};
}
}

#endif
