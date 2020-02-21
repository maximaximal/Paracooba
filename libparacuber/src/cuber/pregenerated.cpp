#include "../../include/paracuber/cuber/pregenerated.hpp"
#include <vector>
#include <cassert>

namespace paracuber {
namespace cuber {
Pregenerated::Pregenerated(ConfigPtr config,
                           LogPtr log,
                           CNF& rootCNF,
                           const messages::JobInitiator& ji)
  : Cuber(config, log, rootCNF)
  , m_ji(ji)
{}

Pregenerated::~Pregenerated() {}

bool
Pregenerated::init()
{
  return true;
}

bool
Pregenerated::shouldGenerateTreeSplit(CNFTree::Path path)
{
  auto var = m_ji.getDecisionOnPath(path);
  return var != 0;
}
void
Pregenerated::getCube(CNFTree::Path path, std::vector<int>& literals)
{
  literals.clear();
  literals.reserve(CNFTree::getDepth(path));

  for(size_t depth = 1; depth < CNFTree::getDepth(path); ++depth) {
    CNFTree::Path currPath = CNFTree::setDepth(path, depth);
    auto lit = m_ji.getDecisionOnPath(CNFTree::cleanupPath(currPath));
    assert(lit != 0);
    literals.push_back(lit);
  }
}

}
}
