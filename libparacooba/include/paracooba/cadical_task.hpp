#ifndef PARACOOBA_CDCLTASK_HPP
#define PARACOOBA_CDCLTASK_HPP

#include "cnftree.hpp"
#include "cuber/cuber.hpp"
#include "task.hpp"
#include "types.hpp"
#include <memory>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/io_service.hpp>


namespace CaDiCaL {
class Solver;
}

namespace paracooba {
class Terminator;
class CNF;
class CaDiCaLMgr;

/** @brief Wraps a CDCL solver process.
 */
class CaDiCaLTask : public Task
{
  public:
  using AssignmentVector = std::vector<uint8_t>;

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
  CaDiCaLTask(boost::asio::io_service& io_service, const TaskResult& result);

  /** @brief Create a new CaDiCaL solver task */
  CaDiCaLTask(boost::asio::io_service& io_service, uint32_t* varCount = nullptr, Mode = ParseAndSolve);

  /** @brief Destructor */
  virtual ~CaDiCaLTask();

  /** @brief Set the solving mode, so parsing or solving can be done
   * independently.
   */
  void setMode(Mode mode);

  /** @brief Copy the internal solver from another CaDiCaLTask.
   */
  void copyFromCaDiCaLTask(const CaDiCaLTask& other);

  void applyCubeFromCuberDeferred(Path p, cuber::Cuber& cuber);
  void applyCubeDeferred(Path, const Cube& cube);

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
  void readCNF(std::shared_ptr<CNF> cnf, Path path);

  /** @brief split the current problem and re-enqueue the split problems */
  std::vector<std::pair<Path, Cube>> resplit(std::chrono::duration<long int, std::ratio<1, 1000000000> >, bool);
  std::vector<std::pair<Path, Cube>> resplit_once(Path path, Cube literals);
  std::vector<std::pair<Path, Cube>> resplit_depth(Path path, Cube literals,
							    std::chrono::duration<long int, std::ratio<1, 1000000000> >,
							    int);

  virtual TaskResultPtr execute();
  virtual void terminate();

  CaDiCaL::Solver& getSolver()
  {
    assert(m_solver);
    return *m_solver;
  }
  uint32_t getVarCount() { return m_internalVarCount; }

  void setRootCNF(std::shared_ptr<CNF> rootCNF) { m_cnf = rootCNF; }

  void setCaDiCaLMgr(CaDiCaLMgr* cadicalMgr) { m_cadicalMgr = cadicalMgr; }

  void releaseSolver();

  void writeEncodedAssignment(AssignmentVector&);
  void writeDecodedAssignment(AssignmentVector&);

  const std::vector<int>& getPregeneratedCubes() const
  {
    return m_pregeneratedCubes;
  }

  void setPregeneratedCubes(std::vector<int>cubes)
  {
    m_pregeneratedCubes = cubes;
  }

  TaskResult::Status lookahead(int, int);

  class FastSplit {
  public:
    operator bool () const {
      return fastSplit;
    }
    void half_tick(bool b) {
      local_situation = b;
    }
    void tick(bool b) {
      assert(beta <= alpha);
      const bool full_tick = (b || local_situation);
      if(full_tick)
        ++beta;
      else
        fastSplit = false;
      ++alpha;

      if(alpha == period) {
        fastSplit = (beta >= period / 2);
        beta = 0;
        alpha = 0;
        if(fastSplit)
          ++depth;
        else
          --depth;
      }
    }
    int split_depth () {
      return fastSplit ? depth : 0;
    }
  private:
    unsigned alpha = 0;
    unsigned beta = 0;
    bool fastSplit = true;
    const unsigned period = 8;
    int depth = 1;
    bool local_situation = true;
  };

  virtual void shouldFastSplit(bool) override;

  private:
  void provideSolver();

  void applyCube(Path p, const Cube& cube);
  void applyCubeFromCuber(Path p, cuber::Cuber& cuber);

  friend class Terminator;
  std::unique_ptr<Terminator> m_terminator;
  std::shared_ptr<CNF> m_cnf;
  Mode m_mode = ParseAndSolve;
  Path m_path = CNFTree::DefaultUninitiatedPath;
  cuber::Cuber* m_cuber = nullptr;
  OptionalCube m_optionalCube;
  std::vector<int> m_pregeneratedCubes;

  std::unique_ptr<CaDiCaL::Solver> m_solver;
  std::string m_sourcePath;
  bool m_terminate = false;
  bool m_interrupt_solving = false;
  uint32_t* m_varCount = nullptr;
  uint32_t m_internalVarCount = 0;

  CaDiCaLMgr* m_cadicalMgr = nullptr;
  boost::asio::steady_timer m_autoStopTimer;
  boost::asio::io_service& m_io_service;
  std::mutex m_solverMutex;
  FastSplit fastSplit;
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
