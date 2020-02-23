#include "../include/paracuber/task_factory.hpp"
#include "../include/paracuber/cadical_mgr.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/cuber/registry.hpp"
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
  /*
    For debugging task insertion. Can throw segfaults because of library
inconsistencies.

  PARACUBER_LOG(m_logger, Trace)
    << "Pushing path " << CNFTree::pathToStrNoAlloc(p) << " with priority "
    << ptr->getPriority();
  */
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
  task->applyCubeFromCuberDeferred(skel->p, m_rootCNF->getCuberRegistry());
  return { std::move(task), skel->originator, skel->getPriority() };
}

size_t
TaskFactory::getNumberOfOffloadedTasks() const
{
  std::shared_lock lock(m_externalTasksSetMapMutex);
  size_t size = 0;
  for(const auto& it : m_externalTasksSetMap) {
    const ExternalTasksSet& exSet = it.second;
    size += exSet.tasks.size();
  }
  return size;
}

void
TaskFactory::addExternallyProcessingTask(int64_t originator,
                                         CNFTree::Path p,
                                         ClusterStatistics::Node& node)
{
  std::unique_lock lock(m_externalTasksSetMapMutex);

  auto it = m_externalTasksSetMap.find(node.getId());
  if(it == m_externalTasksSetMap.end()) {
    ExternalTasksSet exSet{ node };
    auto [insertedIt, inserted] = m_externalTasksSetMap.insert(
      std::make_pair(node.getId(), std::move(exSet)));
    assert(inserted);
    it = insertedIt;

    ExternalTasksSet& set = it->second;

    set.nodeOfflineSignalConnection = node.getNodeOfflineSignal().connect(
      std::bind(&TaskFactory::readdExternalTasks, this, set.node.getId()));
  }
  ExternalTasksSet& set = it->second;
  set.addTask(TaskSkeleton{ CubeOrSolve, originator, p });
}

void
TaskFactory::removeExternallyProcessedTask(CNFTree::Path p, int64_t id)
{
  std::unique_lock lock(m_externalTasksSetMapMutex);

  auto it = m_externalTasksSetMap.find(id);
  if(it != m_externalTasksSetMap.end()) {
    ExternalTasksSet& set = it->second;
    set.removeTask(p);
  }
}

size_t
TaskFactory::ExternalTasksSet::readdTasks(TaskFactory* factory)
{
  assert(factory);
  for(TaskSkeleton skel : tasks) {
    factory->getRootCNF()->getCNFTree().resetNode(skel.p);
    factory->addPath(skel.p, skel.mode, skel.originator);
  }
  size_t count = tasks.size();
  tasks.clear();
  return count;
}

void
TaskFactory::ExternalTasksSet::removeTask(CNFTree::Path p)
{
  for(auto it = tasks.begin(); it != tasks.end();) {
    if(CNFTree::getDepthShiftedPath(CNFTree::setDepth(
         it->p, CNFTree::getDepth(p))) == CNFTree::getDepthShiftedPath(p))
      it = tasks.erase(it);
    else
      ++it;
  }
}

void
TaskFactory::readdExternalTasks(int64_t id)
{
  std::unique_lock lock(m_externalTasksSetMapMutex);

  auto it = m_externalTasksSetMap.find(id);
  if(it != m_externalTasksSetMap.end()) {
    ExternalTasksSet& set = it->second;
    size_t count = set.readdTasks(this);
    m_externalTasksSetMap.erase(it);

    PARACUBER_LOG(m_logger, Trace)
      << "Re-Added " << count
      << " tasks to local factory for task with originator "
      << m_rootCNF->getOriginId() << " which were previously sent to node "
      << id;
  }
}
}
