#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/runner.hpp"
#include <boost/asio.hpp>

namespace paracuber {
Communicator::Communicator(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_ioService(std::make_shared<boost::asio::io_service>())
  , m_logger(log->createLogger())
  , m_signalSet(std::make_unique<boost::asio::signal_set>(*m_ioService, SIGINT))
  , m_runner(std::make_shared<Runner>(config, log))
{
  boost::asio::io_service::work work(*m_ioService);
  m_ioServiceWork = work;

  m_signalSet->async_wait(std::bind(&Communicator::signalHandler,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
}

Communicator::~Communicator()
{
  m_ioService->stop();
}

void
Communicator::run()
{
  if(!m_runner->isRunning()) {
    m_runner->start();
  }
  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service started.";
  m_ioService->run();
  PARACUBER_LOG(m_logger, Trace) << "Communicator io_service ended.";
  m_runner->stop();
}

void
Communicator::startRunner()
{
  m_runner->start();
}

void
Communicator::signalHandler(const boost::system::error_code& error,
                            int signalNumber)
{
  if(signalNumber == SIGINT) {
    PARACUBER_LOG(m_logger, Trace) << "SIGINT detected.";
    m_ioService->stop();
  }
}

}
