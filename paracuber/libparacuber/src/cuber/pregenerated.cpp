#include "../../include/paracuber/cuber/pregenerated.hpp"
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
Pregenerated::generateCube(CNFTree::Path path, CNFTree::CubeVar& var)
{
  var = m_ji.getDecisionOnPath(path);
  return var != 0;
}

}
}
