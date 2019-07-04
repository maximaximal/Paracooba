#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/communicator.hpp"
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <cassert>

#include <iostream>
using std::cout;
using std::endl;

#include <cadical/cadical.hpp>

namespace paracuber {
CaDiCaLTask::CaDiCaLTask()
  : m_solver(std::make_unique<CaDiCaL::Solver>())
{}
CaDiCaLTask::~CaDiCaLTask() {}

void
CaDiCaLTask::readDIMACSFile(std::string_view sourcePath)
{
  m_sourcePath = sourcePath;
}

TaskResultPtr
CaDiCaLTask::execute()
{
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
  PARACUBER_LOG((*m_logger), Trace)
    << "CNF formula parsed with " << vars << " variables.";

  PARACUBER_LOG((*m_logger), Trace)
    << "Start solving CNF formula using CaDiCaL CNF solver.";
  int solveResult = m_solver->solve();
  PARACUBER_LOG((*m_logger), Trace) << "CNF formula solved.";

  TaskResult::Status status;

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

  auto result = std::make_unique<TaskResult>(status);
  return std::move(result);
}
}
