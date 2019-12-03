#include "../include/paracuber/cadical_mgr.hpp"
#include <algorithm>
#include <cassert>

#include <cadical/cadical.hpp>

namespace paracuber {
CaDiCaLMgr::CaDiCaLMgr() {}
CaDiCaLMgr::~CaDiCaLMgr() {}

void
CaDiCaLMgr::setRootTask(CaDiCaLTask* rootTask)
{
  assert(m_solvers.size() > 0);
  m_rootTask = rootTask;

  std::generate(m_solvers.begin(), m_solvers.end(), [rootTask]() {
    auto solver = std::make_unique<CaDiCaL::Solver>();
    rootTask->getSolver().copy(*solver);
    return solver;
  });
}
void
CaDiCaLMgr::initWorkerSlots(size_t workers)
{
  m_solvers.resize(workers);
}

std::unique_ptr<CaDiCaL::Solver>
CaDiCaLMgr::getSolverForWorker(uint32_t workerId)
{
  assert(workerId < m_solvers.size());
  assert(m_solvers[workerId]);
  return std::move(m_solvers[workerId]);
}
void
CaDiCaLMgr::returnSolverFromWorker(std::unique_ptr<CaDiCaL::Solver> task,
                                   uint32_t workerId)
{
  assert(workerId < m_solvers.size());
  assert(!m_solvers[workerId]);
  m_solvers[workerId] = std::move(task);
}
}
