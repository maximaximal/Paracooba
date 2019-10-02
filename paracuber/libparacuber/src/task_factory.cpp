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
  m_config->getCommunicator()->getRunner()->registerTaskFactory(this);
}
TaskFactory::~TaskFactory()
{
  m_config->getCommunicator()->getRunner()->deregisterTaskFactory(this);
}

void
TaskFactory::addPath(CNFTree::Path p, Mode mode, int64_t originator)
{
  std::unique_lock lock(m_skeletonsMutex);
  m_skeletons.push(TaskSkeleton{ mode, originator, p });
  ++m_availableTasks;
}
std::pair<std::unique_ptr<Task>, int64_t>
TaskFactory::produceTask()
{
  if(canProduceTask()) {
    --m_availableTasks;
  } else {
    return { nullptr, 0 };
  }

  TaskSkeleton skel;
  {
    std::unique_lock lock(m_skeletonsMutex);
    skel = std::move(m_skeletons.front());
    m_skeletons.pop();
  }

  switch(skel.mode) {
    case CubeOrSolve:
      return produceCubeOrSolveTask(std::move(skel));
      break;
    case Solve:
      return produceSolveTask(std::move(skel));
      break;
    default:
      break;
  }

  return { nullptr, skel.originator };
}

std::pair<std::unique_ptr<Task>, int64_t>
TaskFactory::produceCubeOrSolveTask(TaskSkeleton skel)
{
  std::unique_ptr<DecisionTask> task =
    std::make_unique<DecisionTask>(m_rootCNF, skel.p);
  return { std::move(task), skel.originator };
}
std::pair<std::unique_ptr<Task>, int64_t>
TaskFactory::produceSolveTask(TaskSkeleton skel)
{
  CaDiCaLTask* rootTask = m_rootCNF->getRootTask();
  assert(rootTask);
  std::unique_ptr<CaDiCaLTask> task = std::make_unique<CaDiCaLTask>(*rootTask);
  task->setMode(CaDiCaLTask::Solve);
  task->applyPathFromCNFTree(skel.p, m_rootCNF->getCNFTree());
  return { std::move(task), skel.originator };
}
}
