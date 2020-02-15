#ifndef PARACUBER_TASKFACTORY_HPP
#define PARACUBER_TASKFACTORY_HPP

#include <boost/signals2/connection.hpp>
#include <queue>

#include "cluster-statistics.hpp"
#include "cnftree.hpp"
#include "log.hpp"
#include "paracuber/priority_queue_lock_semantics.hpp"
#include <atomic>
#include <mutex>

namespace paracuber {
class Task;
class CaDiCaLMgr;
class CaDiCaLTask;

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
      return getPriority() > b.getPriority();
    }

    inline bool operator==(const TaskSkeleton& b) const
    {
      return mode == b.mode && originator == b.originator && p == b.p;
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
  ProducedTask produceTaskBackwards();

  TaskSkeleton produceTaskSkeleton();
  TaskSkeleton produceTaskSkeletonBackwards();

  void addExternallyProcessingTask(int64_t originator,
                                   CNFTree::Path p,
                                   ClusterStatistics::Node& node);

  void removeExternallyProcessedTask(CNFTree::Path p, int64_t id);
  void readdExternalTasks(int64_t id);

  inline bool canProduceTask() const { return !m_skeletons.empty(); }
  inline uint64_t getSize() const { return m_skeletons.size(); }
  int64_t getOriginId() const;
  std::shared_ptr<CNF> getRootCNF() const { return m_rootCNF; }

  void setRootTask(CaDiCaLTask* rootTask);
  void initWorkerSlots(size_t workers);

  ProducedTask produceCubeOrSolveTask(std::unique_ptr<TaskSkeleton> skel);
  ProducedTask produceSolveTask(std::unique_ptr<TaskSkeleton> skel);

  CaDiCaLMgr* getCaDiCaLMgr()
  {
    assert(m_cadicalMgr);
    return m_cadicalMgr.get();
  }

  private:
  struct ExternalTasksSet
  {
    ClusterStatistics::Node& node;
    boost::signals2::scoped_connection nodeOfflineSignalConnection;
    std::set<TaskSkeleton> tasks;

    size_t readdTasks(TaskFactory* factory);
    void addTask(TaskSkeleton&& skel) { tasks.insert(std::move(skel)); }
    void removeTask(CNFTree::Path p);
  };
  using ExternalTasksSetMap = std::map<int64_t, ExternalTasksSet>;

  ConfigPtr m_config;
  LoggerMT m_logger;
  PriorityQueueLockSemanticsUniquePtr<TaskSkeleton> m_skeletons;
  ExternalTasksSetMap m_externalTasksSetMap;
  std::shared_ptr<CNF> m_rootCNF;
  std::unique_ptr<CaDiCaLMgr> m_cadicalMgr;

  std::mutex m_externalTasksSetMapMutex;
};
}

#endif
