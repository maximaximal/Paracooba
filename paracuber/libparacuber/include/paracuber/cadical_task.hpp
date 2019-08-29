#ifndef PARACUBER_CDCLTASK_HPP
#define PARACUBER_CDCLTASK_HPP

#include "task.hpp"

namespace CaDiCaL {
class Solver;
}

namespace paracuber {
class Terminator;
class CNF;

/** @brief Wraps a CDCL solver process.
 */
class CaDiCaLTask : public Task
{
  public:
  enum Mode
  {
    Solve,
    ParseOnly
  };

  /** @brief Construct this new CaDiCaL task as the copy of another CaDiCaL task
   *
   * Copies the solver internally, so this can be used to minify parsing time.
   */
  CaDiCaLTask(const CaDiCaLTask& other);

  /** @brief Initiate this task from the result of another task, copying the old
   * solver.
   *
   * This only works with results of a CaDiCaL task! The contained task is read
   * and all important options are applied to this new task.
   */
  CaDiCaLTask(const TaskResult& result);

  /** @brief Create a new CaDiCaL solver task */
  CaDiCaLTask(uint32_t* varCount = nullptr, Mode = Solve);

  /** @brief Destructor */
  virtual ~CaDiCaLTask();

  void copyFromCaDiCaLTask(const CaDiCaLTask& other);

  /** @brief Queue parsing a DIMACS file into the internal solver instance.
   *
   * This returns immediately and the file is only parsed once the task has been
   * started.
   */
  void readDIMACSFile(std::string_view sourcePath);

  /** @brief Read the given CNF and generate task from there.
   *
   * The root CNF gets parsed directly, other CNFs (cubes) are applied to the
   * current state. This makes this function useful in conjunction with
   * initialising this task from an old task that already finished. */
  void readCNF(std::shared_ptr<CNF> cnf);

  virtual TaskResultPtr execute();
  virtual void terminate();

  private:
  friend class Terminator;
  std::unique_ptr<Terminator> m_terminator;
  Mode m_mode = Solve;

  std::unique_ptr<CaDiCaL::Solver> m_solver;
  std::string m_sourcePath;
  bool m_terminate = false;
  uint32_t* m_varCount = nullptr;
};
}

#endif
