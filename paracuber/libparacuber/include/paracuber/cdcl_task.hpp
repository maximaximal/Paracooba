#ifndef PARACUBER_CDCLTASK_HPP
#define PARACUBER_CDCLTASK_HPP

#include "task.hpp"

namespace CaDiCaL {
class Solver;
}

namespace paracuber {
/** @brief Wraps a CDCL solver process.
 */
class CDCLTask : public Task
{
  public:
  CDCLTask();
  virtual ~CDCLTask();

  /** @brief Read DIMACS file into the internal solver instance. */
  const char* readDIMACSFile(std::string_view sourcePath);

  virtual TaskResultPtr execute();

  private:
  std::unique_ptr<CaDiCaL::Solver> m_solver;
  bool m_ready = false;
};
}

#endif
