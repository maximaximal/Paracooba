#pragma once

#include <memory>
#include <string>

namespace parac::solver_qbf {
class GenericSolverHandle {
  public:
  GenericSolverHandle();
  ~GenericSolverHandle();

  virtual std::unique_ptr<GenericSolverHandle> from_file(
    const std::string& file_path);

  virtual std::unique_ptr<GenericSolverHandle> from_generic_handle(
    GenericSolverHandle& handle);

  private:
  std::string m_path;
};
}
