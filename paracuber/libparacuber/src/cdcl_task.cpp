#include "../include/paracuber/cdcl_task.hpp"
#include <cadical/cadical.hpp>
#include <boost/filesystem.hpp>
#include <cassert>

namespace paracuber {
CDCLTask::CDCLTask()
  : m_solver(std::make_unique<CaDiCaL::Solver>())
{
}
CDCLTask::~CDCLTask() {}

const char*
CDCLTask::readDIMACSFile(std::string_view sourcePath)
{
  if(!boost::filesystem::exists(std::string(sourcePath))) {
    return "file does not exist";
  }

  int vars = 0;
  const char* status =
    m_solver->read_dimacs(std::string(sourcePath).c_str(), vars, 1);
  if(status == 0) {
    m_ready = true;
  }
  return status;
}

TaskResultPtr
CDCLTask::execute()
{
  if(!m_ready) {
    return std::move(std::make_unique<TaskResult>(TaskResult::MissingInputs));
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
