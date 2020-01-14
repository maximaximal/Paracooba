#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/log.hpp"
#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/task_factory.hpp"
#include <shared_mutex>

namespace paracuber {
Daemon::Context::Context(std::shared_ptr<CNF> rootCNF,
                         int64_t originatorID,
                         Daemon* daemon,
                         ClusterStatistics::Node& statsNode)
  : m_rootCNF(rootCNF)
  , m_originatorID(originatorID)
  , m_daemon(daemon)
  , m_logger(daemon->m_log->createLogger("Daemon"))
  , m_statisticsNode(statsNode)
  , m_taskFactory(
      std::make_unique<TaskFactory>(daemon->m_config, daemon->m_log, rootCNF))
{
  PARACUBER_LOG(m_logger, Trace)
    << "Create new context with origin " << m_originatorID << ".";
  m_rootCNF->setTaskFactory(m_taskFactory.get());
}
Daemon::Context::~Context()
{
  PARACUBER_LOG(m_logger, Trace)
    << "Destroy context with origin " << m_originatorID;
  m_rootCNF->setTaskFactory(nullptr);
}

void
Daemon::Context::start(State change)
{
  // This is called whenever a CNF was received - so whenever new data is here,
  // the state can be checked again.

  std::unique_lock lock(m_contextMutex);

  if(change == FormulaReceived &&
     (m_state == 0 ||
      (m_state & AllowanceMapReceived && !(m_state & FormulaReceived)))) {
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

      // Formula has been parsed successfully and local solver has been started.
      // The internal CNF can therefore now be fully initialised with this root
      // CNF solver task. The root CNF now has a valid unique_ptr to a
      // completely parsed CaDiCaL task. This is (in theory) unsafe, but should
      // only be required in this case. It should not be required to const cast
      // the result anywhere else, except in client.
      auto& resultMut = const_cast<TaskResult&>(result);
      m_rootCNF->setRootTask(static_unique_pointer_cast<CaDiCaLTask>(
        std::move(resultMut.getTaskPtr())));

      m_taskFactory->setRootTask(m_rootCNF->getRootTask());

      start(FormulaParsed);
      // Once the CNF is parsed, this compute node sits idle until cubes arrive.
      // These cubes can then be cubed further or just solved directly,
      // depending on the heuristics of the current compute node.
    });
    m_daemon->m_communicator->getRunner()->push(
      std::move(task), m_originatorID, 0, m_taskFactory.get());
  } else if(change == FormulaReceived && m_state & WaitingForWork) {
    // Received a cubing task! Submit it to the local cubing factory. This is
    // done inside the CNF class.
  } else if(change == FormulaParsed) {
    m_state = m_state | FormulaParsed;
  } else if(change == AllowanceMapReceived) {
    m_state = m_state | AllowanceMapReceived;
  }

  if(m_state & FormulaParsed && m_state & AllowanceMapReceived &&
     change != FormulaReceived) {
    // Ready to start receiving cubes!
    m_state = m_state | WaitingForWork;
    PARACUBER_LOG(m_logger, Trace) << "Ready for Work!";
  }

  m_statisticsNode.setContextState(m_originatorID, m_state);
}
uint64_t
Daemon::Context::getFactoryQueueSize() const
{
  return m_taskFactory->getSize();
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

SharedLockView<Daemon::Context*>
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
