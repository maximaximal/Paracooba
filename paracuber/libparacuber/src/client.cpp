#include "../include/paracuber/client.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/communicator.hpp"

#include <cadical/cadical.hpp>

namespace paracuber {
Client::Client(ConfigPtr config, LogPtr log, CommunicatorPtr communicator)
  : m_config(config)
  , m_solver(std::make_shared<CaDiCaL::Solver>())
  , m_logger(log->createLogger())
  , m_communicator(communicator)
{}
Client::~Client() {}

const char*
Client::readDIMACSFromConfig()
{
  std::string_view sourcePath = m_config->getString(Config::InputFile);
  if(sourcePath == "") {
    static const char* errorMsg = "No input file given!";
    PARACUBER_LOG(m_logger, LocalWarning) << errorMsg;
    return errorMsg;
  }
  return readDIMACS(sourcePath);
}

const char*
Client::readDIMACS(std::string_view sourcePath)
{
  int vars = 0;
  const char* status =
    m_solver->read_dimacs(std::string(sourcePath).c_str(), vars, 1);
  return status;
}

int
Client::solve()
{
  return m_solver->solve();
}

}
