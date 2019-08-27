#include "../include/paracuber/client.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/runner.hpp"

namespace paracuber {
Client::Client(ConfigPtr config, LogPtr log, CommunicatorPtr communicator)
  : m_config(config)
  , m_logger(log->createLogger())
  , m_communicator(communicator)
{
  m_config->m_client = this;
  m_rootCNF = std::make_shared<CNF>(0, getDIMACSSourcePathFromConfig());
}
Client::~Client() {}

std::string_view
Client::getDIMACSSourcePathFromConfig()
{
  std::string_view sourcePath = m_config->getString(Config::InputFile);
  if(sourcePath == "") {
    static const char* errorMsg = "No input file given!";
    PARACUBER_LOG(m_logger, LocalWarning) << errorMsg;
    return errorMsg;
  }
  return sourcePath;
}

void
Client::solve()
{
  auto task = std::make_unique<CaDiCaLTask>();
  auto& finishedSignal = task->getFinishedSignal();
  task->readDIMACSFile(getDIMACSSourcePathFromConfig());
  m_communicator->getRunner()->push(std::move(task));
  finishedSignal.connect([this](const TaskResult& result) {
    m_status = result.getStatus();
    // Finished solving, the communicator can be stopped!
    m_communicator->exit();
  });
}

}
