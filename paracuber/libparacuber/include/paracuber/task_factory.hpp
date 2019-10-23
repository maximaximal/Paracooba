#ifndef PARACUBER_TASKFACTORY_HPP
#define PARACUBER_TASKFACTORY_HPP

#include <queue>

#include "cnftree.hpp"
#include "log.hpp"
#include "paracuber/priority_queue_lock_semantics.hpp"
#include <atomic>
#include <mutex>

namespace paracuber {
class Task;

/** @brief Creates new tasks for the runners during the solving process.
 *
 * Every newly produced path is given to this factory. It then decides if
 * another cube shall be generated or if the path should directly be given to a
 * CDCL solver. It is only doing local actions, so selection of the correct
 * compute node must happen externally.
 */
class TaskFactory
{
  public:
  explicit TaskFactory(ConfigPtr config,
                       LogPtr log,
                       std::shared_ptr<CNF> rootCNF);
  ~TaskFactory();

  enum Mode
  {
    CubeOrSolve,
    Solve
  };

  struct TaskSkeleton
  {
    Mode mode;
    int64_t originator;
    CNFTree::Path p;

    TaskSkeleton(Mode mode, int64_t originator, CNFTree::Path p)
      : mode(mode)
      , originator(originator)
      , p(p)
    {}

    inline int getPriority() const
    {
      return (CNFTree::maxPathDepth - CNFTree::getDepth(p)) +
             (mode != Solve ? 100 : 0);
    }

    inline bool operator<(TaskSkeleton const& b) const
    {
      return getPriority() < b.getPriority();
    }
  };

  struct ProducedTask
  {
    std::unique_ptr<Task> task;
    int64_t originator;
    int priority;
  };

  void addPath(CNFTree::Path p, Mode m, int64_t originator);

  ProducedTask produceTask();
  inline bool canProduceTask() const { return !m_skeletons.empty(); }
  inline uint64_t getSize() const { return m_skeletons.size(); }
  int64_t getOriginId() const;

  private:
  ProducedTask produceCubeOrSolveTask(std::unique_ptr<TaskSkeleton> skel);
  ProducedTask produceSolveTask(std::unique_ptr<TaskSkeleton> skel);

  ConfigPtr m_config;
  Logger m_logger;
  PriorityQueueLockSemanticsUniquePtr<TaskSkeleton> m_skeletons;
  std::shared_ptr<CNF> m_rootCNF;
};
}

#endif
