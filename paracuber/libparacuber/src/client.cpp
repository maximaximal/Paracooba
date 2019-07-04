#include "../include/paracuber/client.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/runner.hpp"

namespace paracuber {
Client::Client(ConfigPtr config, LogPtr log, CommunicatorPtr communicator)
  : m_config(config)
  , m_logger(log->createLogger())
  , m_communicator(communicator)
{}
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

TaskResult::Status
Client::solve()
{
  auto task = std::make_unique<CaDiCaLTask>();
  task->readDIMACSFile(getDIMACSSourcePathFromConfig());
  auto future = m_communicator->getRunner()->push(std::move(task));
  auto resultValue = future.get();
  return resultValue->getStatus();
}

}
