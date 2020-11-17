#pragma once

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

struct parac_task;

namespace parac::solver {
class CaDiCaLManager;

class SolverTask {
  public:
  SolverTask();
  ~SolverTask();

  void init(CaDiCaLManager& manager, parac_task& task);

  parac_status work(parac_worker worker);

  static SolverTask& createRoot(parac_task& task, CaDiCaLManager& manager);
  static SolverTask& create(parac_task& task, CaDiCaLManager& manager);
  static parac_status static_work(parac_task* task, parac_worker worker);

  private:
  CaDiCaLManager* m_manager = nullptr;
  parac_task* m_task;
};
}
