#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/log.hpp"
#include "../include/paracuber/runner.hpp"

namespace paracuber {
Daemon::Context::Context(std::shared_ptr<CNF> rootCNF,
                         int64_t originatorID,
                         Daemon* daemon,
                         ClusterStatistics::Node& statsNode)
  : m_rootCNF(rootCNF)
  , m_originatorID(originatorID)
  , m_daemon(daemon)
  , m_logger(daemon->m_log->createLogger())
  , m_statisticsNode(statsNode)
{
  PARACUBER_LOG(m_logger, Trace)
    << "Create new context with origin " << m_originatorID << ".";
}
Daemon::Context::~Context()
{
  PARACUBER_LOG(m_logger, Trace)
    << "Destroy context with origin " << m_originatorID;
}

void
Daemon::Context::start()
{
  // The context should be started! After creating a context, the root DIMACS
  // file that created this context must be parsed. This is done in a new
  // CaDiCaL task.

  auto task = std::make_unique<CaDiCaLTask>(nullptr, CaDiCaLTask::Parse);
  task->readCNF(m_rootCNF, 0);

  auto& finishedSignal = task->getFinishedSignal();

  finishedSignal.connect([this](const TaskResult& result) {
    if(result.getStatus() != TaskResult::Parsed) {
      PARACUBER_LOG(m_logger, LocalWarning)
        << "Could not parse the given formula! This could stem from a "
           "transmission error or from an invalid formula. Solving cannot "
           "continue on this node.";
      return;
    }
    // Once the CNF is parsed, this compute node sits idle until cubes arrive.
    // These cubes can then be cubed further or just solved directly, depending
    // on the heuristics of the current compute node.
  });

  m_daemon->m_communicator->getRunner()->push(std::move(task));
}

Daemon::Daemon(ConfigPtr config,
               LogPtr log,
               std::shared_ptr<Communicator> communicator)
  : m_config(config)
  , m_log(log)
  , m_communicator(communicator)
{
  m_config->m_daemon = this;
}
Daemon::~Daemon()
{
  m_config->m_daemon = nullptr;
}

Daemon::Context*
Daemon::getContext(int64_t id)
{
  auto it = m_contextMap.find(id);
  if(it != m_contextMap.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::pair<Daemon::Context&, bool>
Daemon::getOrCreateContext(std::shared_ptr<CNF> rootCNF, int64_t id)
{
  if(m_contextMap.count(id) > 0) {
    return { *m_contextMap.find(id)->second, false };
  } else {
    auto [statsNode, inserted] =
      m_communicator->getClusterStatistics()->getOrCreateNode(id);

    auto p = std::make_pair(
      id, std::make_unique<Context>(rootCNF, id, this, statsNode));
    Context& context = *p.second;
    m_contextMap.insert(std::move(p));
    return { context, true };
  }
}
}
