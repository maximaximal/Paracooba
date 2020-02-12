#include "../include/paracuber/task_factory.hpp"
#include "../include/paracuber/cadical_mgr.hpp"
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
  , m_logger(log->createLoggerMT("TaskFactory"))
  , m_rootCNF(rootCNF)
  , m_cadicalMgr(std::make_unique<CaDiCaLMgr>())
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
  auto ptr = std::make_unique<TaskSkeleton>(mode, originator, p);
  PARACUBER_LOG(m_logger, Trace)
    << "Pushing path " << CNFTree::pathToStrNoAlloc(p) << " with priority "
    << ptr->getPriority();
  m_skeletons.push(std::move(ptr));
}

template<class Functor>
TaskFactory::ProducedTask
parametricProduceTask(
  PriorityQueueLockSemanticsUniquePtr<TaskFactory::TaskSkeleton>& queue,
  TaskFactory& factory,
  Functor f)
{
  std::unique_lock lock(queue.getMutex());
  if(queue.empty()) {
    return { nullptr, 0, 0 };
  }
  std::unique_ptr<TaskFactory::TaskSkeleton> skel = f(queue);
  lock.unlock();// Early unlock: This could happen in parallel.

  switch(skel->mode) {
    case TaskFactory::CubeOrSolve:
      return factory.produceCubeOrSolveTask(std::move(skel));
      break;
    case TaskFactory::Solve:
      return factory.produceSolveTask(std::move(skel));
      break;
    default:
      break;
  }

  return { nullptr, skel->originator, 0 };
}

std::unique_ptr<TaskFactory::TaskSkeleton>
popFromFrontOfQueue(
  PriorityQueueLockSemanticsUniquePtr<TaskFactory::TaskSkeleton>& queue)
{
  return queue.popNoLock();
}
std::unique_ptr<TaskFactory::TaskSkeleton>
popFromBackOfQueue(
  PriorityQueueLockSemanticsUniquePtr<TaskFactory::TaskSkeleton>& queue)
{
  return queue.popBackNoLock();
}

TaskFactory::ProducedTask
TaskFactory::produceTask()
{
  return parametricProduceTask(m_skeletons, *this, &popFromFrontOfQueue);
}

TaskFactory::TaskSkeleton
TaskFactory::produceTaskSkeleton()
{
  return std::move(*m_skeletons.pop());
}

TaskFactory::TaskSkeleton
TaskFactory::produceTaskSkeletonBackwards()
{
  return std::move(*m_skeletons.popBack());
}

TaskFactory::ProducedTask
TaskFactory::produceTaskBackwards()
{
  return parametricProduceTask(m_skeletons, *this, &popFromBackOfQueue);
}

int64_t
TaskFactory::getOriginId() const
{
  return m_rootCNF->getOriginId();
}

void
TaskFactory::setRootTask(CaDiCaLTask* rootTask)
{
  assert(m_cadicalMgr);
  m_cadicalMgr->setRootTask(rootTask);
}
void
TaskFactory::initWorkerSlots(size_t workers)
{
  assert(m_cadicalMgr);
  m_cadicalMgr->initWorkerSlots(workers);
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
  assert(m_cadicalMgr);
  std::unique_ptr<CaDiCaLTask> task = std::make_unique<CaDiCaLTask>(*rootTask);
  task->setMode(CaDiCaLTask::Solve);
  task->setCaDiCaLMgr(m_cadicalMgr.get());
  task->applyPathFromCNFTreeDeferred(skel->p, m_rootCNF->getCNFTree());
  return { std::move(task), skel->originator, skel->getPriority() };
}
}