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
  m_rootCNF = std::make_shared<CNF>(
    log, config->getInt64(Config::Id), getDIMACSSourcePathFromConfig());
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
  CaDiCaLTask::Mode mode = CaDiCaLTask::Parse;

  auto task = std::make_unique<CaDiCaLTask>(&m_cnfVarCount, mode);
  auto& finishedSignal = task->getFinishedSignal();
  task->readDIMACSFile(getDIMACSSourcePathFromConfig());
  m_communicator->getRunner()->push(std::move(task));
  finishedSignal.connect([this](const TaskResult& result) {
    m_status = result.getStatus();
    if(m_status != TaskResult::Parsed) {
      PARACUBER_LOG(m_logger, Fatal)
        << "Could not parse DIMACS source file! Status: " << result.getStatus()
        << ". Exiting Client.";
      m_communicator->exit();
    }

    // If local solving is enabled, create a new task to solve the parsed
    // formula.
    if(m_config->isClientCaDiCaLEnabled()) {
      auto task = std::make_unique<CaDiCaLTask>(
        static_cast<CaDiCaLTask&&>(result.getTask()));
      task->setMode(CaDiCaLTask::Solve);
      auto& finishedSignal = task->getFinishedSignal();
      task->readDIMACSFile(getDIMACSSourcePathFromConfig());
      m_communicator->getRunner()->push(std::move(task));
      finishedSignal.connect([this](const TaskResult& result) {
        // Finished solving using client CaDiCaL!
        m_status = result.getStatus();
        m_communicator->exit();
      });
    }
  });
}
}
