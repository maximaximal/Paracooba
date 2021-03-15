#pragma once

#include <memory>

#include <paracooba/common/task.h>

struct parac_handle;

namespace parac::solver {
class CaDiCaLManager;

class KissatTask {
  public:
  KissatTask(parac_handle& handle,
             const char* file,
             parac_task& task,
             CaDiCaLManager& cadicalManager);
  ~KissatTask();

  private:
  static parac_status static_work(parac_task* task, parac_worker worker);
  static void static_terminate(parac_task* task);
  static parac_status static_free_userdata(parac_task* task);

  parac_status work(parac_worker worker);
  void terminate();

  struct Internal;
  std::unique_ptr<Internal> m_internal;

  const char* m_file;
  parac_task& m_task;
};
}
