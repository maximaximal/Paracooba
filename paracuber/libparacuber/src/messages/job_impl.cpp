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
  assert(getCubingKind() == PregeneratedCubes);
  auto& map = getCubeMap();
  size_t i = 0;

  CNFTree::Path p = 0;
  int cubeVar = 0;
  for(auto var : flatCubeArr) {
    if(var == 0) {
      // Cube finished.
      auto [_, inserted] = map.insert(std::make_pair(p, cubeVar));
      if(!inserted) {
        // Already inserted previously! This breaks the binary tree property of
        // the provided cubes!
        return i;
      }
      p = 0;
      ++i;
      continue;
    } else {
      // Inside cube.

      if(var > 0) {
        p = CNFTree::getNextRightPath(p);
      } else {
        p = CNFTree::getNextLeftPath(p);
      }
    }
    cubeVar = FastAbsolute(var);

    auto [currVar, inserted] = map.insert(std::make_pair(p, cubeVar));
    if(currVar->second != cubeVar) {
      // Existing path has different value at this position! This breaks the
      // binary tree property.
      return i;
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
