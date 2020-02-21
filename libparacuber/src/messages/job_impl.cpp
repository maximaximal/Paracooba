#include "../../include/paracuber/cnf.hpp"
#include "../../include/paracuber/cnftree.hpp"
#include "../../include/paracuber/messages/job_initiator.hpp"
#include "../../include/paracuber/messages/job_path.hpp"
#include "../../include/paracuber/messages/job_result.hpp"
#include "../../include/paracuber/util.hpp"

namespace paracuber {
namespace messages {
std::string
JobInitiator::tagline() const
{
  return "JobInitiator{" +
         std::to_string(static_cast<CNF::CubingKind>(getCubingKind())) + "}";
}
size_t
JobInitiator::realise(const std::vector<int>& flatCubeArr)
{
  auto& map = initCubeMap();
  size_t i = 0;

  CNFTree::Path p = 0, lastPath = 0;
  int cubeVar = 0;
  for(auto var : flatCubeArr) {
    if(var == 0) {
      // Cube finished.
      p = 0;
      ++i;
      continue;
    } else {
      // Inside cube.

      cubeVar = FastAbsolute(var);
      assert(cubeVar != 0);

      auto [currVar, _] =
        map.insert(std::make_pair(CNFTree::cleanupPath(p), cubeVar));
      if(currVar->second != cubeVar) {
        // Existing path has different value at this position! This breaks the
        // binary tree property.
        return i;
      }

      if(var > 0) {
        p = CNFTree::getNextRightPath(p);
      } else {
        p = CNFTree::getNextLeftPath(p);
      }
    }
  }
  return 0;
}

std::string
JobPath::tagline() const
{
  return "JobPath{" + CNFTree::pathToStdString(getPath()) + "}";
}
std::string
JobResult::tagline() const
{
  return "JobResult{" + CNFTree::pathToStdString(getPath()) + "," +
         std::to_string(getState()) + "}";
}

std::ostream&
operator<<(std::ostream& o, JobResult::State s)
{
  switch(s) {
    case JobResult::State::SAT:
      o << "SAT";
      break;
    case JobResult::State::UNSAT:
      o << "UNSAT";
      break;
    case JobResult::State::UNKNOWN:
      o << "UNKNOWN";
      break;
    default:
      o << "(! UNKNOWN JobResult::STATE !)";
      break;
  }
  return o;
}
}
}
