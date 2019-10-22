#include "../include/paracuber/task_factory.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/decision_task.hpp"
#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/task.hpp"

namespace paracuber {
TaskFactory::TaskFactory(ConfigPtr config,
                         LogPtr log,
                         std::shared_ptr<CNF> rootCNF)
  : m_config(config)
  , m_logger(log->createLogger())
  , m_rootCNF(rootCNF)
{
  if(m_config->hasCommunicator()) {
    m_config->getCommunicator()->getRunner()->registerTaskFactory(this);
  }
}
TaskFactory::~TaskFactory()
{
  if(m_config->hasCommunicator()) {
    m_config->getCommunicator()->getRunner()->deregisterTaskFactory(this);
  }
}

void
TaskFactory::addPath(CNFTree::Path p, Mode mode, int64_t originator)
{
  m_skeletons.push(std::make_unique<TaskSkeleton>(mode, originator, p));
}
TaskFactory::ProducedTask
TaskFactory::produceTask()
{
  std::unique_lock lock(m_skeletons.getMutex());
  if(m_skeletons.empty()) {
    return { nullptr, 0, 0 };
  }
  auto skel(std::move(m_skeletons.popNoLock()));
  lock.unlock();// Early unlock: This could happen in parallel.

  switch(skel->mode) {
    case CubeOrSolve:
      return produceCubeOrSolveTask(std::move(skel));
      break;
    case Solve:
      return produceSolveTask(std::move(skel));
      break;
    default:
      break;
  }

  return { nullptr, skel->originator, 0 };
}

int64_t
TaskFactory::getOriginId() const
{
  return m_rootCNF->getOriginId();
}

TaskFactory::ProducedTask
TaskFactory::produceCubeOrSolveTask(std::unique_ptr<TaskSkeleton> skel)
{
  std::unique_ptr<DecisionTask> task =
    std::make_unique<DecisionTask>(m_rootCNF, skel->p);
  return { std::move(task), skel->originator, skel->getPriority() };
}
TaskFactory::ProducedTask
TaskFactory::produceSolveTask(std::unique_ptr<TaskSkeleton> skel)
{
  CaDiCaLTask* rootTask = m_rootCNF->getRootTask();
  assert(rootTask);
  std::unique_ptr<CaDiCaLTask> task = std::make_unique<CaDiCaLTask>(*rootTask);
  task->setMode(CaDiCaLTask::Solve);
  task->applyPathFromCNFTree(skel->p, m_rootCNF->getCNFTree());
  return { std::move(task), skel->originator, skel->getPriority() };
}
}
