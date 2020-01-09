#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/assignment_serializer.hpp"
#include "../include/paracuber/cadical_mgr.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <cassert>

#include <iostream>
using std::cout;
using std::endl;

#include <cadical/cadical.hpp>

namespace paracuber {
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
{
  copyFromCaDiCaLTask(other);
  m_name = "CaDiCaL Task (copied)";
}
CaDiCaLTask::CaDiCaLTask(const TaskResult& result)
  : CaDiCaLTask(nullptr, ParseAndSolve)
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
{
  m_terminator->setCaDiCaLTask(this);
  m_name = "CaDiCaL Task (moved)";
}

CaDiCaLTask::CaDiCaLTask(uint32_t* varCount, Mode mode)
  : m_terminator(std::make_unique<Terminator>(this))
  , m_varCount(varCount)
  , m_mode(mode)
  , m_solver(std::make_unique<CaDiCaL::Solver>())
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
CaDiCaLTask::applyPathFromCNFTreeDeferred(CNFTree::Path p, const CNFTree& tree)
{
  m_name = "Solver Task for Path " + CNFTree::pathToStdString(p);
  m_path = p;
}

void
CaDiCaLTask::applyPathFromCNFTree(CNFTree::Path p, const CNFTree& tree)
{
  provideSolver();

  tree.visit(
    CNFTree::setDepth(p, CNFTree::getDepth(p) - 1),
    [this](
      CNFTree::CubeVar p, uint8_t depth, CNFTree::State state, int64_t remote) {
      m_solver->assume(p);
      return false;
    });

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
CaDiCaLTask::readCNF(std::shared_ptr<CNF> cnf, CNFTree::Path path)
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
    applyPathFromCNFTree(m_path, m_cnf->getCNFTree());
  }

  TaskResult::Status status;
  if(m_mode & Parse) {
    if(!boost::filesystem::exists(m_sourcePath)) {
      return std::move(std::make_unique<TaskResult>(TaskResult::MissingInputs));
    }

    PARACUBER_LOG((*m_logger), Trace)
      << "Start parsing CNF formula in DIMACS format from \"" << m_sourcePath
      << "\".";
    int vars = 0;
    const char* parse_status =
      m_solver->read_dimacs(m_sourcePath.c_str(), vars, 1);
    if(parse_status != 0) {
      return std::move(std::make_unique<TaskResult>(TaskResult::ParsingError));
    }

    if(m_varCount != nullptr) {
      *m_varCount = vars;
    }
    m_internalVarCount = vars;

    PARACUBER_LOG((*m_logger), Trace)
      << "CNF formula parsed with " << vars << " variables.";
    status = TaskResult::Parsed;
  }

  if(m_mode & Mode::Solve) {
    PARACUBER_LOG((*m_logger), Trace)
      << "Start solving CNF formula using CaDiCaL CNF solver.";
    int solveResult = m_solver->solve();
    PARACUBER_LOG((*m_logger), Trace)
      << "CNF formula for path " << CNFTree::pathToStrNoAlloc(m_path)
      << " solved.";

    switch(solveResult) {
      case 0:
        status = TaskResult::Unsolved;
        break;
      case 10:
        status = TaskResult::Satisfiable;
        break;
      case 20:
        status = TaskResult::Unsatisfiable;
        break;
    }
  }

  auto result = std::make_unique<TaskResult>(status);
  return std::move(result);
}

void
CaDiCaLTask::terminate()
{
  m_terminate = true;
}
void
CaDiCaLTask::releaseSolver()
{
  assert(m_cadicalMgr);
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
}
