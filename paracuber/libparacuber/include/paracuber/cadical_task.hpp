#ifndef PARACUBER_CDCLTASK_HPP
#define PARACUBER_CDCLTASK_HPP

#include "task.hpp"

namespace CaDiCaL {
class Solver;
}

namespace paracuber {
/** @brief Wraps a CDCL solver process.
 */
class CaDiCaLTask : public Task
{
  public:
  CaDiCaLTask();
  virtual ~CaDiCaLTask();

  /** @brief Queue parsing a DIMACS file into the internal solver instance.
   *
   * This returns immediately and the file is only parsed once the task has been started.
   */
  void readDIMACSFile(std::string_view sourcePath);

  virtual TaskResultPtr execute();

  private:
  std::unique_ptr<CaDiCaL::Solver> m_solver;
  std::string m_sourcePath;
};
}

#endif
