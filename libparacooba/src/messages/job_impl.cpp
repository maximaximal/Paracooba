#include "../../include/paracooba/cnf.hpp"
#include "../../include/paracooba/cnftree.hpp"
#include "../../include/paracooba/messages/job_initiator.hpp"
#include "../../include/paracooba/messages/job_path.hpp"
#include "../../include/paracooba/messages/job_result.hpp"
#include "../../include/paracooba/util.hpp"

namespace paracooba {
namespace messages {
std::string
JobInitiator::tagline() const
{
  return "JobInitiator{" +
         std::to_string(static_cast<CNF::CubingKind>(getCubingKind())) + "}";
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
