#ifndef PARACUBER_CUBER_NAIVE_CUTTER_HPP
#define PARACUBER_CUBER_NAIVE_CUTTER_HPP

#include "cuber.hpp"
#include <vector>

namespace paracuber {
namespace cuber {
/** @brief Splits by most to least commonly occurring literal.
 *
 * The naive cutter tries to find the most common literal and uses that
 * for the next decision.
 */
class NaiveCutter : public Cuber
{
  public:
  explicit NaiveCutter(ConfigPtr config, LogPtr log, CNF& rootCNF);
  virtual ~NaiveCutter();

  using ClauseMap = std::vector<size_t>;

  virtual CNFTree::CubeVar generateCube(CNFTree::Path path);

  private:
  ClauseMap m_clauseFrequency;
};
}
}

#endif
