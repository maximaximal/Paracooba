#ifndef PARACOOBA_CUBER_NAIVE_CUTTER_HPP
#define PARACOOBA_CUBER_NAIVE_CUTTER_HPP

#include "../config.hpp"
#include "cuber.hpp"
#include <vector>

namespace paracooba {
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

  bool init();

  virtual bool shouldGenerateTreeSplit(CNFTree::Path path);
  virtual bool getCube(CNFTree::Path path, std::vector<int>& literals);

  Cuber::LiteralMap* getLiteralFrequency() { return m_literalFrequency; }

  private:
  Cuber::LiteralMap* m_literalFrequency;
  size_t m_counter = 0;
};
}
}

#endif
