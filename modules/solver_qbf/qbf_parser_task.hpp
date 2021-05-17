#pragma once

#include <string_view>

#include <paracooba/common/status.h>
#include <paracooba/common/task.h>
#include <paracooba/common/types.h>

struct parac_task;
struct parac_handle;

namespace parac::solver_qbf {
class Parser;

class QBFParserTask {
  public:
  using FinishedCB = std::function<void(parac_status, std::unique_ptr<Parser>)>;

  explicit QBFParserTask(parac_handle& handle,
                         parac_task& task,
                         std::string_view input,
                         FinishedCB cb = nullptr);
  ~QBFParserTask();

  private:
  parac_handle& m_handle;
  parac_task& m_task;
  FinishedCB m_finishedCB;

  static parac_status static_work(parac_task* self, parac_worker worker);
  parac_status work(parac_worker worker);

  std::unique_ptr<Parser> m_parser;
};
}
