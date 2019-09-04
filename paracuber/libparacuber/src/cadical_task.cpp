#include "../include/paracuber/cadical_task.hpp"
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

  private:
  CaDiCaLTask* m_task;
};

CaDiCaLTask::CaDiCaLTask(const CaDiCaLTask& other)
{
  copyFromCaDiCaLTask(other);
}
CaDiCaLTask::CaDiCaLTask(const TaskResult& result)
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

CaDiCaLTask::CaDiCaLTask(uint32_t* varCount, Mode mode)
  : m_terminator(std::make_unique<Terminator>(this))
  , m_solver(std::make_unique<CaDiCaL::Solver>())
  , m_varCount(varCount)
  , m_mode(mode)
{
  m_solver->connect_terminator(m_terminator.get());
}
CaDiCaLTask::~CaDiCaLTask() {}

void
CaDiCaLTask::copyFromCaDiCaLTask(const CaDiCaLTask& other)
{}

void
CaDiCaLTask::readDIMACSFile(std::string_view sourcePath)
{
  m_sourcePath = sourcePath;
}

void
CaDiCaLTask::readCNF(std::shared_ptr<CNF> cnf)
{
  /// This applies the CNF tree to the current solver object. This means, that
  /// the solver applies all missing rules from the CNF tree to the current
  /// context. See @ref CNFTree for more.
  if(cnf->getPrevious() == 0) {
    // Root CNF - this must be parsed only. The root CNF never has to be solved
    // directly, as this is done by the client.
    m_mode = Parse;
    readDIMACSFile(cnf->getDimacsFile());
  } else {
    // Apply the CNF tree to the current solver.
  }
}

TaskResultPtr
CaDiCaLTask::execute()
{
  TaskResult::Status status;
  if(m_mode | Parse) {
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

    PARACUBER_LOG((*m_logger), Trace)
      << "CNF formula parsed with " << vars << " variables.";
    status = TaskResult::Parsed;
  }

  if(m_mode | Mode::Solve) {
    PARACUBER_LOG((*m_logger), Trace)
      << "Start solving CNF formula using CaDiCaL CNF solver.";
    int solveResult = m_solver->solve();
    PARACUBER_LOG((*m_logger), Trace) << "CNF formula solved.";

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
}
