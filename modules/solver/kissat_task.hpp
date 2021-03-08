#pragma once

#include <memory>

struct parac_handle;

namespace parac::solver {
class KissatTask {
  public:
  KissatTask(parac_handle& handle, const char* file);
  ~KissatTask();

  private:
  struct Internal;
  std::unique_ptr<Internal> m_internal;

  const char* m_file;
};
}
