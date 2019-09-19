#ifndef PARACUBER_CUBER_NAIVE_CUTTER_HPP
#define PARACUBER_CUBER_NAIVE_CUTTER_HPP

#include "cuber.hpp"
#include "../config.hpp"
#include <vector>

namespace paracuber {
namespace cuber {
/** @brief Generates a literal frequency map.
 *
 * If this is used as a cuber, the next most frequently used literal is used to
 * cut the formula.
 */
class LiteralFrequency : public Cuber
{
  public:
  explicit LiteralFrequency(ConfigPtr config,
                            LogPtr log,
                            CNF& rootCNF,
                            Cuber::LiteralMap* allowanceMap);
  virtual ~LiteralFrequency();

  virtual bool generateCube(CNFTree::Path path, CNFTree::CubeVar& var);

  Cuber::LiteralMap* getLiteralFrequency() { return m_literalFrequency; }

  private:
  Cuber::LiteralMap* m_literalFrequency;
  size_t m_counter = 0;
};
}
}

#endif
