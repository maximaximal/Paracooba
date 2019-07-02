#include "../include/paracuber/cadical_task.hpp"
#include <boost/filesystem.hpp>
#include <cadical/cadical.hpp>
#include <cassert>

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

  int vars = 0;
  const char* parse_status = m_solver->read_dimacs(m_sourcePath.c_str(), vars, 1);
  if(parse_status != 0) {
    return std::move(std::make_unique<TaskResult>(TaskResult::ParsingError));
  }
  int solveResult = m_solver->solve();

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
