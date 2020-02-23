#include "../../include/paracuber/cuber/pregenerated.hpp"
#include <cassert>
#include <vector>

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

  for(size_t depth = 1; depth <= CNFTree::getDepth(path); ++depth) {
    CNFTree::Path currPath = CNFTree::setDepth(path, depth);
    CNFTree::Path parentPath = CNFTree::setDepth(currPath, depth - 1);
    auto lit = m_ji.getDecisionOnPath(CNFTree::cleanupPath(parentPath)) *
               (CNFTree::getAssignment(currPath, depth) ? 1 : -1);

    /*
    PARACUBER_LOG(m_logger, Trace)
      << "Debug for path " << CNFTree::pathToStrNoAlloc(path)
      << ": Literal on path " << CNFTree::pathToStdString(currPath) << " is "
      << lit;
    */

    assert(lit != 0);
    literals.push_back(lit);
  }
}

}
}
