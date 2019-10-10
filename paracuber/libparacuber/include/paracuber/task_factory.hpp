#ifndef PARACUBER_TASKFACTORY_HPP
#define PARACUBER_TASKFACTORY_HPP

#include <queue>

#include "cnftree.hpp"
#include "log.hpp"
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
  };

  void addPath(CNFTree::Path p, Mode m, int64_t originator);

  std::pair<std::unique_ptr<Task>, int64_t> produceTask();
  inline bool canProduceTask() const { return m_availableTasks > 0; }
  inline uint64_t getSize() const { return m_availableTasks; }
  int64_t getOriginId() const;

  private:
  std::pair<std::unique_ptr<Task>, int64_t> produceCubeOrSolveTask(
    TaskSkeleton skel);
  std::pair<std::unique_ptr<Task>, int64_t> produceSolveTask(TaskSkeleton skel);

  ConfigPtr m_config;
  Logger m_logger;
  std::mutex m_skeletonsMutex;
  std::queue<TaskSkeleton> m_skeletons;
  std::shared_ptr<CNF> m_rootCNF;

  std::atomic<uint64_t> m_availableTasks = 0;
};
}

#endif
