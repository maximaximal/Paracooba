#pragma once

struct parac_task;

namespace parac::solver {
class SolverTask {
  public:
  static void create(parac_task& task);

  static void work(parac_task* task);
  private:
};
}
