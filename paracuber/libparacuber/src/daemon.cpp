#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/log.hpp"
#include "../include/paracuber/runner.hpp"
#include <shared_mutex>

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
Daemon::Context::start(State change)
{
  // This is called whenever a CNF was received - so whenever new data is here,
  // the state can be checked again.

  std::unique_lock lock(m_contextMutex);

  if(change == FormulaReceived) {
    m_state = m_state | FormulaReceived;

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

      start(FormulaParsed);
      // Once the CNF is parsed, this compute node sits idle until cubes arrive.
      // These cubes can then be cubed further or just solved directly,
      // depending on the heuristics of the current compute node.
    });
    m_daemon->m_communicator->getRunner()->push(std::move(task));
  } else if(change == FormulaParsed) {
    m_state = m_state | FormulaParsed;
  } else if(change == AllowanceMapReceived) {
    m_state = m_state | AllowanceMapReceived;
  }

  if(m_state & FormulaParsed && m_state & AllowanceMapReceived) {
    // Ready to start receiving cubes!
    m_state = m_state | WaitingForWork;
    PARACUBER_LOG(m_logger, Trace) << "Ready for Work!";
  }
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

std::pair<const Daemon::ContextMap&, std::shared_lock<std::shared_mutex>>
Daemon::getContextMap()
{
  std::shared_lock sharedLock(m_contextMapMutex);
  return { m_contextMap, std::move(sharedLock) };
}

std::pair<Daemon::Context*, std::shared_lock<std::shared_mutex>>
Daemon::getContext(int64_t id)
{
  std::shared_lock sharedLock(m_contextMapMutex);
  auto it = m_contextMap.find(id);
  if(it != m_contextMap.end()) {
    return { it->second.get(), std::move(sharedLock) };
  }
  return { nullptr, std::move(sharedLock) };
}

std::pair<Daemon::Context&, bool>
Daemon::getOrCreateContext(int64_t id)
{
  std::unique_lock uniqueLock(m_contextMapMutex);
  auto it = m_contextMap.find(id);
  bool inserted = false;
  if(it == m_contextMap.end()) {
    // Create the new context with a new CNF.
    auto rootCNF = std::make_shared<CNF>(m_config, m_log, id);
    auto [statsNode, inserted] =
      m_communicator->getClusterStatistics()->getOrCreateNode(id);
    auto p = std::make_pair(
      id, std::make_unique<Context>(rootCNF, id, this, statsNode));
    inserted = true;
    auto [contextIt, contextInserted] = m_contextMap.insert(std::move(p));
    it = contextIt;
  }
  return { *it->second, inserted };
}
}
