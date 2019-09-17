#ifndef PARACUBER_CDCLTASK_HPP
#define PARACUBER_CDCLTASK_HPP

#include "cnftree.hpp"
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
    Solve = 0b00000001,
    Parse = 0b00000010,
    ParseAndSolve = Solve | Parse,
  };

  /** @brief Construct this new CaDiCaL task as the copy of another CaDiCaL task
   *
   * Copies the solver internally, so this can be used to minify parsing time.
   */
  CaDiCaLTask(const CaDiCaLTask& other);

  /** @brief Move-Construct this new CaDiCaL task as a direct copy, without
   * creating too much new memory.
   */
  CaDiCaLTask(CaDiCaLTask&& other);

  /** @brief Initiate this task from the result of another task, copying the old
   * solver.
   *
   * This only works with results of a CaDiCaL task! The contained task is read
   * and all important options are applied to this new task.
   */
  CaDiCaLTask(const TaskResult& result);

  /** @brief Create a new CaDiCaL solver task */
  CaDiCaLTask(uint32_t* varCount = nullptr, Mode = ParseAndSolve);

  /** @brief Destructor */
  virtual ~CaDiCaLTask();

  void setMode(Mode mode);

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
  void readCNF(std::shared_ptr<CNF> cnf, CNFTree::Path path);

  virtual TaskResultPtr execute();
  virtual void terminate();

  private:
  friend class Terminator;
  std::unique_ptr<Terminator> m_terminator;
  Mode m_mode = ParseAndSolve;

  std::unique_ptr<CaDiCaL::Solver> m_solver;
  std::string m_sourcePath;
  bool m_terminate = false;
  uint32_t* m_varCount = nullptr;
};

inline CaDiCaLTask::Mode
operator|(CaDiCaLTask::Mode a, CaDiCaLTask::Mode b)
{
  return static_cast<CaDiCaLTask::Mode>(static_cast<int>(a) |
                                        static_cast<int>(b));
}
inline CaDiCaLTask::Mode operator&(CaDiCaLTask::Mode a, CaDiCaLTask::Mode b)
{
  return static_cast<CaDiCaLTask::Mode>(static_cast<int>(a) &
                                        static_cast<int>(b));
}

}

#endif
