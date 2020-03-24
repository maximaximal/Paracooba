#ifndef PARACOOBA_CADICALMGR_HPP
#define PARACOOBA_CADICALMGR_HPP

#include <cstdint>
#include <map>
#include <memory>

#include "cadical_task.hpp"

namespace CaDiCaL {
class Solver;
}

namespace paracooba {
class CaDiCaLMgr
{
  public:
  CaDiCaLMgr();
  ~CaDiCaLMgr();

  void setRootTask(CaDiCaLTask* rootTask);
  void initWorkerSlots(size_t workers);

  std::unique_ptr<CaDiCaL::Solver> getSolverForWorker(uint32_t workerId);
  void returnSolverFromWorker(std::unique_ptr<CaDiCaL::Solver> task,
                              uint32_t workerId);


  private:
  CaDiCaLTask* m_rootTask = nullptr;

  std::vector<std::unique_ptr<CaDiCaL::Solver>> m_solvers;
};
}

#endif
