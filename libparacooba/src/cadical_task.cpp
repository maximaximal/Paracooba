#include "../include/paracooba/cadical_task.hpp"
#include "../include/paracooba/assignment_serializer.hpp"
#include "../include/paracooba/cadical_mgr.hpp"
#include "../include/paracooba/cnf.hpp"
#include "../include/paracooba/communicator.hpp"
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <cassert>

#include <iostream>
#include <algorithm>

using std::cout;
using std::endl;

#include <cadical/cadical.hpp>

namespace paracooba {
class Terminator : public CaDiCaL::Terminator
{
  public:
  Terminator(CaDiCaLTask* task)
    : m_task(task)
  {}
  virtual ~Terminator() {}

  virtual bool terminate() { return m_task->m_terminate; }

  void setCaDiCaLTask(CaDiCaLTask* task) { m_task = task; }

  private:
  CaDiCaLTask* m_task;
};

CaDiCaLTask::CaDiCaLTask(const CaDiCaLTask& other)
  : m_terminator(std::make_unique<Terminator>(this))
  , m_varCount(other.m_varCount)
  , m_mode(other.m_mode)
  , m_cadicalMgr(other.m_cadicalMgr)
  , m_autoStopTimer(other.m_cnf->getIOService())
  , m_io_service(other.m_cnf->getIOService())
{
  copyFromCaDiCaLTask(other);
  m_name = "CaDiCaL Task (copied)";
}
CaDiCaLTask::CaDiCaLTask(boost::asio::io_service& io_service, const TaskResult& result)
  : CaDiCaLTask(io_service, nullptr, ParseAndSolve)
{
  Task& otherTask = result.getTask();
  if(CaDiCaLTask* otherCaDiCaLTask = dynamic_cast<CaDiCaLTask*>(&otherTask)) {
    copyFromCaDiCaLTask(*otherCaDiCaLTask);
  } else {
    // This should never happen, programmer has to make sure, only a CaDiCaL
    // task result is given to this constructor.
    assert(false);
  }
}
CaDiCaLTask::CaDiCaLTask(CaDiCaLTask&& other)
  : m_terminator(std::move(other.m_terminator))
  , m_solver(std::move(other.m_solver))
  , m_cnf(std::move(other.m_cnf))
  , m_cadicalMgr(other.m_cadicalMgr)
  , m_autoStopTimer(other.m_io_service)
  , m_io_service(other.m_io_service)
{
  m_terminator->setCaDiCaLTask(this);
  m_name = "CaDiCaL Task (moved)";
}


CaDiCaLTask::CaDiCaLTask(boost::asio::io_service& io_service, uint32_t* varCount, Mode mode)
  : m_terminator(std::make_unique<Terminator>(this))
  , m_varCount(varCount)
  , m_mode(mode)
  , m_solver(std::make_unique<CaDiCaL::Solver>())
  , m_autoStopTimer(io_service)
  , m_io_service(io_service)
{
  m_name = "CaDiCaL Task (completely new)";
}

CaDiCaLTask::~CaDiCaLTask() {}

void
CaDiCaLTask::setMode(Mode mode)
{
  m_mode = mode;
}

void
CaDiCaLTask::copyFromCaDiCaLTask(const CaDiCaLTask& other)
{
  assert(m_terminator);

  m_mode = other.m_mode;
  m_cnf = other.m_cnf;
  m_cadicalMgr = other.m_cadicalMgr;
}

void
CaDiCaLTask::applyCubeFromCuberDeferred(Path p, cuber::Cuber& cuber)
{
  m_name = "Solver Task for Path " + CNFTree::pathToStdString(p);
  m_path = p;
  m_cuber = &cuber;
  m_optionalCube = std::nullopt;
}

void
CaDiCaLTask::applyCubeDeferred(Path p, const Cube& cube)
{
  m_name =
    "Solver Task for Path " + CNFTree::pathToStdString(p) + " with given cube";
  m_path = p;
  m_optionalCube = cube;
  m_cuber = nullptr;
}

void
CaDiCaLTask::applyCube(Path p, const Cube& cube)
{
  provideSolver();

  assert(p != CNFTree::DefaultUninitiatedPath);

  PARACOOBA_LOG((*m_logger), Trace)
    << "Applying from cube " << CNFTree::pathToStrNoAlloc(m_path) << " ";

  for(int lit : cube) {
    m_solver->assume(lit);
  }

  // This means, that the task was assigned from a factory, a callback must be
  // given.
  assert(m_cnf);
  m_finishedSignal.connect(
    std::bind(&CNF::solverFinishedSlot, m_cnf, std::placeholders::_1, p));
}

void
CaDiCaLTask::applyCubeFromCuber(Path p, cuber::Cuber& cuber)
{
  provideSolver();

  assert(p != CNFTree::DefaultUninitiatedPath);

  Cube cube;
  cube.reserve(CNFTree::getDepth(p));
  cuber.getCube(p, cube);

  PARACOOBA_LOG((*m_logger), Trace)
    << "Applying from supplied cube " << CNFTree::pathToStrNoAlloc(m_path) << " ";

  for(int lit : cube) {
    m_solver->assume(lit);
  }

  // This means, that the task was assigned from a factory, a callback must be
  // given.
  assert(m_cnf);
  m_finishedSignal.connect(
    std::bind(&CNF::solverFinishedSlot, m_cnf, std::placeholders::_1, p));
}


void
CaDiCaLTask::readDIMACSFile(std::string_view sourcePath)
{
  m_sourcePath = sourcePath;
}

void
CaDiCaLTask::readCNF(std::shared_ptr<CNF> cnf, Path path)
{
  m_cnf = cnf;
  m_path = path;
  /// This applies the CNF tree to the current solver object. This means, that
  /// the solver applies all missing rules from the CNF tree to the current
  /// context. See @ref CNFTree for more.

  /// This function is executed in the execute() function, execution will
  /// therefore be deferred until the task is actually run!
}

TaskResultPtr
CaDiCaLTask::execute()
{
  provideSolver();

  if(m_path == 0) {
    // Root CNF - this must be parsed only. The root CNF never has to be solved
    // directly, as this is done by the client.
    m_mode = Parse;
    readDIMACSFile(m_cnf->getDimacsFile());
  } else if(m_path != CNFTree::DefaultUninitiatedPath) {
    if(m_cuber) {
      assert(m_cuber);
      applyCubeFromCuber(m_path, *m_cuber);
    } else {
      assert(m_optionalCube.has_value());
      applyCube(m_path, m_optionalCube.value());
    }
  }

  TaskResult::Status status;
  if(m_mode & Parse) {
    if(!boost::filesystem::exists(m_sourcePath)) {
      return std::move(std::make_unique<TaskResult>(TaskResult::MissingInputs));
    }

    PARACOOBA_LOG((*m_logger), Trace)
      << "Start parsing CNF formula in DIMACS format from \"" << m_sourcePath
      << "\".";
    int vars = 0;
    bool incremental = false;
    const char* parse_status = m_solver->read_dimacs(
      m_sourcePath.c_str(), vars, 1, incremental, m_pregeneratedCubes);

    if(parse_status != 0) {
      return std::move(std::make_unique<TaskResult>(TaskResult::ParsingError));
    }

    if(incremental) {
      PARACOOBA_LOG((*m_logger), Trace)
        << "Incremental DIMCAS encountered! Pregenerated cube array size: "
        << BytePrettyPrint(m_pregeneratedCubes.size() * sizeof(int))
        << " (will be parsed at a later stage)";
    }

    if(m_varCount != nullptr) {
      *m_varCount = vars;
    }
    m_internalVarCount = vars;

    PARACOOBA_LOG((*m_logger), Trace)
      << "CNF formula parsed with " << vars << " variables.";
    status = TaskResult::Parsed;
  }

  m_internalVarCount = m_solver->vars();

  if(m_mode & Mode::Solve) {
    std::chrono::duration<double> average_time = std::chrono::duration_cast<std::chrono::milliseconds>(m_cnf->averageSolvingTime());
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(average_time * 3);
    PARACOOBA_LOG((*m_logger), Trace)
      << "Start solving CNF formula using CaDiCaL CNF solver "
      << "for roughly "
      << duration.count()
      << "ms.";

    m_interrupt_solving = false;
    if(m_cnf->shouldResplitCubes() &&
       CNFTree::getDepth(m_path) < CNFTree::maxPathDepth) {
      m_autoStopTimer.expires_from_now(duration);
      m_autoStopTimer.async_wait([this](const boost::system::error_code& errc) {
        std::lock_guard lock(m_solverMutex);
        if(errc != boost::asio::error::operation_aborted && m_solver){
          PARACOOBA_LOG((*m_logger), Cubes)
            << "CNF formula for path " << CNFTree::pathToStrNoAlloc(m_path)
            << " will be interrupted.";
          m_solver->terminate();
          //m_finishedSignal.disconnect_all_slots();
          m_interrupt_solving = true;
        }
      });
    }
    auto start = std::chrono::steady_clock::now();
    int solveResult = m_solver->solve();
    auto end = std::chrono::steady_clock::now();
    m_autoStopTimer.cancel();

    PARACOOBA_LOG((*m_logger), Cubes)
      << "Stopped solving CNF formula using CaDiCaL CNF solver "
      << "after roughly "
      << std::chrono::duration<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
         1'000 * (end - start))).count()
      << "ms.";
    m_cnf->update_averageSolvingTime(std::chrono::duration<double>(std::chrono::duration_cast<std::chrono::milliseconds>((end - start))));

    if(!m_interrupt_solving) {
      PARACOOBA_LOG((*m_logger), Trace)
        << "CNF formula for path " << CNFTree::pathToStrNoAlloc(m_path)
        << " solved with result " << solveResult;
    }

    switch(solveResult) {
    case 0:
      {
        m_interrupt_solving = false;
        auto start = std::chrono::steady_clock::now();
        auto new_cubes = resplit(duration);
        m_interrupt_solving = false;
        auto end = std::chrono::steady_clock::now();
        PARACOOBA_LOG((*m_logger), Cubes)
          << " splitting took "
          << std::chrono::duration<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
              1'000 * (end - start))).count() << "ms";

        if(m_terminate) {
          status = TaskResult::Unsolved;
          break;
        } else if(new_cubes.size()) {
          status = TaskResult::Resplitted;
          m_cnf->getCNFTree().setStateFromLocal(m_path, CNFTree::Working);
          for(auto&& [path, cube] : new_cubes)
            m_cnf->addPath(path, m_originator, cube);
        } else // very unlikely, but still
          status = TaskResult::Unsatisfiable;
        auto result = std::make_unique<TaskResult>(status);
        return std::move(result);
        break;
      }
    case 10:
      status = TaskResult::Satisfiable;
      break;
    case 20:
      status = TaskResult::Unsatisfiable;
      break;
    default:
      status = TaskResult::Unsolved;
      break;
    }
  }

  auto result = std::make_unique<TaskResult>(status);
  return std::move(result);
}

std::vector<std::pair<Path, Cube>>
CaDiCaLTask::resplit_once(Path path, Cube literals,
			  std::chrono::duration<long int, std::ratio<1, 1000000000> > duration)
{
  assert(m_solver);
  PARACOOBA_LOG((*m_logger), Cubes)
    << "CNF formula for path " << CNFTree::pathToStrNoAlloc(path)
    << " resplitted.";
  assert(!m_interrupt_solving);

  Cube literals2(literals);
  for(auto lit : literals) {
    PARACOOBA_LOG((*m_logger), Cubes)
      << "cube lit: " << lit;
    m_solver->assume(lit);
  }

  Literal lit_to_split = m_solver->lookahead();
  m_solver->reset_assumptions();
  m_autoStopTimer.cancel();
  assert(lit_to_split != 0);
  if(lit_to_split == INT_MIN) {
    PARACOOBA_LOG((*m_logger), Cubes)
      << "CNF formula for path " << CNFTree::pathToStrNoAlloc(path)
      << " is UNSAT";
    return std::vector<std::pair<Path, Cube>>{};
  }
  PARACOOBA_LOG((*m_logger), Cubes)
    << "CNF formula for path " << CNFTree::pathToStrNoAlloc(path)
    << " resplitted on literal " << lit_to_split;
  literals.push_back(lit_to_split);
  literals2.push_back(-lit_to_split);
  m_solver->reset_assumptions();
  return std::vector<std::pair<Path, Cube>>
    {{CNFTree::getNextLeftPath(path), literals},
     {CNFTree::getNextRightPath(path), literals2}};
}

std::vector<std::pair<Path, Cube>>
CaDiCaLTask::resplit_depth(Path path, Cube literals,
			   std::chrono::duration<long int, std::ratio<1, 1000000000> > duration,
			   int depth)
{
  Cube literals2(literals);
  for(auto lit : literals) {
    PARACOOBA_LOG((*m_logger), Cubes)
      << "cube lit" << lit;
    m_solver->assume(lit);
  }
  m_autoStopTimer.expires_from_now(duration);
  m_autoStopTimer.async_wait([this, path](const boost::system::error_code& errc) {
    std::lock_guard lock(m_solverMutex);
    if(errc != boost::asio::error::operation_aborted && m_solver){
      PARACOOBA_LOG((*m_logger), Cubes)
        << "CNF lookahead for path " << CNFTree::pathToStrNoAlloc(path)
        << " will be interrupted.";
      m_solver->terminate();
      m_interrupt_solving = true;
    }
  });

  std::vector<std::pair<Path, Cube>> cubes{resplit_once(path, literals, duration)};
  m_autoStopTimer.cancel();

  for(int i = 0; i < depth && !m_interrupt_solving; ++i)
  {
    auto cubes2{std::move(cubes)};
    for(auto && [p, cube] : cubes2) {
      for(auto &&pcube : resplit_once(p, cube, duration))
        cubes.push_back(pcube);
    }
  }
  return cubes;
}

std::vector<std::pair<Path, Cube>>
CaDiCaLTask::resplit(std::chrono::duration<long int, std::ratio<1, 1000000000> > duration)
{
  assert(m_solver);
  PARACOOBA_LOG((*m_logger), Cubes)
    << "CNF formula for path " << CNFTree::pathToStrNoAlloc(m_path)
    << " must be resplitted.";
  assert(!m_interrupt_solving);
  m_autoStopTimer.expires_from_now(duration);
  m_autoStopTimer.async_wait([this](const boost::system::error_code& errc) {
    std::lock_guard lock(m_solverMutex);
    if(errc != boost::asio::error::operation_aborted && m_solver){
      PARACOOBA_LOG((*m_logger), Cubes)
        << "CNF lookahead for path " << CNFTree::pathToStrNoAlloc(m_path)
        << " will be interrupted.";
      m_solver->terminate();
      m_interrupt_solving = true;
    }
  });

  if(m_cuber) {
    Cube literals;
    m_cuber->getCube(m_path, literals);
    return resplit_depth(m_path, literals, duration, 0);
  } else {
    assert(m_optionalCube.has_value());
    Cube literals(m_optionalCube.value());
    return resplit_depth(m_path, literals, duration, 0);
  }
}

void
CaDiCaLTask::terminate()
{
  Task::terminate();
  // Terminating resets the cadical manager, as it no longer exists at this
  // point.

  PARACOOBA_LOG((*m_logger), Trace)
    << "Terminating instance";
  m_cadicalMgr = nullptr;
  m_interrupt_solving = true;
  m_terminate = true;
}
void
CaDiCaLTask::releaseSolver()
{
  std::lock_guard lock(m_solverMutex);
  m_solver->disconnect_terminator();
  if(!m_cadicalMgr)
    return;
  m_cadicalMgr->returnSolverFromWorker(std::move(m_solver), m_workerId);
}

void
CaDiCaLTask::writeEncodedAssignment(AssignmentVector& encodedAssignment)
{
  assert(m_solver);
  assert(m_solver->state() == CaDiCaL::SATISFIED);
  m_internalVarCount = m_solver->vars();

  SerializeAssignmentFromSolver(
    encodedAssignment, m_internalVarCount, *m_solver);
}

void
CaDiCaLTask::writeDecodedAssignment(AssignmentVector& decodedAssignment)
{
  assert(m_solver);
  assert(m_solver->state() == CaDiCaL::SATISFIED);
  m_internalVarCount = m_solver->vars();
  decodedAssignment.resize(m_internalVarCount);

  for(int i = 0; i < m_internalVarCount; ++i)
    decodedAssignment[i] = static_cast<bool>(m_solver->val(i + 1));
}

void
CaDiCaLTask::provideSolver()
{
  if(m_solver)
    return;

  assert(m_cadicalMgr);

  m_solver = m_cadicalMgr->getSolverForWorker(m_workerId);

  assert(m_solver);

  m_solver->connect_terminator(m_terminator.get());
}

TaskResult::Status
CaDiCaLTask::lookahead(int depth)
{
  using namespace std::chrono_literals;

  PARACOOBA_LOG((*m_logger), Cubes)
    << "Generating cubes of length " << depth << " ";

  assert(m_solver);
  assert(m_pregeneratedCubes.empty());
  m_autoStopTimer.expires_from_now(30s);
  m_autoStopTimer.async_wait(
    [this](const boost::system::error_code& errc) {
      std::lock_guard lock(m_solverMutex);
      if(errc != boost::asio::error::operation_aborted && m_solver) {
        PARACOOBA_LOG((*m_logger), Cubes)
          << "CNF cubing will be interrupted.";
        m_solver->terminate();
        m_interrupt_solving = true;
      }
    });
  auto cubes {m_solver->generate_cubes(depth)};
  m_autoStopTimer.cancel();
  m_interrupt_solving = false;
  if(cubes.status == 20)
    return TaskResult::Unsatisfiable;
  std::vector<int> flatCubes;
  size_t max_depth = 0;

  for(const auto & cube : cubes.cubes) {
    std::for_each(begin(cube), end(cube),
                  [this](int lit) {m_pregeneratedCubes.emplace_back(lit);});
    m_pregeneratedCubes.emplace_back(0);
    max_depth = std::max(max_depth, cube.size());
  }

  PARACOOBA_LOG((*m_logger), Trace)
    << "Generated " << cubes.cubes.size() << " cubes. Max depth = " << max_depth;
  return TaskResult::Unknown;
}
}
