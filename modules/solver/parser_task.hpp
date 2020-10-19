#pragma once

#include <memory>
#include <string>

struct parac_task;
struct parac_handle;

namespace parac::solver {
class ParserTask {
  public:
  ParserTask(parac_handle& handle, const std::string& file);
  ~ParserTask();

  parac_task& task();

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  parac_handle& m_handle;
  const std::string m_file;
};
}
