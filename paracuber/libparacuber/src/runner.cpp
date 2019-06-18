#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/task.hpp"

#include <algorithm>

namespace paracuber {
Runner::Runner(ConfigPtr config, LogPtr log, CommunicatorPtr communicator)
  : m_config(config)
  , m_log(log)
  , m_communicator(communicator)
  , m_logger(log->createLogger())
{}
Runner::~Runner()
{
  end();
}

void
Runner::start()
{
  m_running = true;
  for(uint32_t i = 0; i < m_config->getUint32(Config::ThreadCount); ++i) {
    m_pool.push_back(
      std::thread(std::bind(&Runner::worker, this, i, m_log->createLogger())));
  }
}

void
Runner::end()
{
  m_running = false;
  std::for_each(m_pool.begin(), m_pool.end(), [](auto& t) { t.join(); });
}

std::future<TaskResult>
Runner::push(std::unique_ptr<Task> task)
{

}

void
Runner::worker(uint32_t workerId, Logger logger)
{
  PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " started.";
  while(m_running) {
  }
  PARACUBER_LOG(logger, Trace) << "Worker " << workerId << " ended.";
}
}
