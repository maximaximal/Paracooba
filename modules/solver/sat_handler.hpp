#pragma once

#include <memory>
#include <mutex>

#include <paracooba/common/types.h>

struct parac_module;

namespace parac {
struct NoncopyOStringstream;
}

namespace parac::solver {
class SolverAssignment;

class SatHandler {
  public:
  SatHandler(parac_module& solverModule, parac_id originatorId);
  ~SatHandler();

  void handleSatisfyingAssignmentFound(
    std::unique_ptr<SolverAssignment> assignment);

  private:
  parac_module& m_mod;
  parac_id m_originatorId;
  std::unique_ptr<SolverAssignment> m_assignment;
  std::unique_ptr<NoncopyOStringstream> m_assignmentOstream;
  std::mutex m_mutex;
};
}
