#include "../include/paracuber/communicator.hpp"
#include <boost/asio.hpp>

namespace paracuber {
Communicator::Communicator(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_logger(log->createLogger())
  , m_ioService(std::make_shared<boost::asio::io_service>())
{
  boost::asio::io_service::work work(*m_ioService);
  m_ioServiceWork = work;
}

Communicator::~Communicator()
{
  m_ioService->stop();
}

void
Communicator::run()
{
  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service started.";
  m_ioService->run();
  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service ended.";
}

}
