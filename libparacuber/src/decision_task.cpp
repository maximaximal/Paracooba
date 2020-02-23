#include "../include/paracuber/decision_task.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/cluster-statistics.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/cuber/registry.hpp"
#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/task_factory.hpp"
#include <cassert>

namespace paracuber {
DecisionTask::DecisionTask(std::shared_ptr<CNF> rootCNF, CNFTree::Path p)
  : m_rootCNF(rootCNF)
  , m_path(p)
{
  m_name = "DecisionTask for Path " + CNFTree::pathToStdString(p);
}
DecisionTask::~DecisionTask() {}

TaskResultPtr
DecisionTask::execute()
{
  /// Steps to take:
  ///   1. Get the cuber::Registry object.
  ///   2. Ask whatever algorithm is active for next decision.
  ///   3. Determine success.
  ///   4. Build new solver task immediately if the decision was negative and
  ///   submit it to local runner, or send the new path to wherever it fits.
  assert(m_rootCNF->readyToBeStarted());
  assert(m_factory);// This task requires to be run with a valid factory!

  auto& cnfTree = m_rootCNF->getCNFTree();
  auto clusterStatistics = m_config->getCommunicator()->getClusterStatistics();

  CNFTree::State state = cnfTree.getState(m_path);
  if(state != CNFTree::Unvisited && state != CNFTree::UnknownPath) {
    PARACUBER_LOG((*m_logger), GlobalError)
      << "Processing decision task on path "
      << CNFTree::pathToStrNoAlloc(m_path)
      << " that is not completely fresh and was visited before! State of node "
         "on this path: "
      << state
      << ". This task ends immediately without further action and returns a "
         "TaskResult::PathAlreadyVisitedError result.";
    return std::make_unique<TaskResult>(TaskResult::PathAlreadyVisitedError);
  }

  cnfTree.setStateFromLocal(m_path, CNFTree::Working);

  if(m_rootCNF->getCuberRegistry().shouldGenerateTreeSplit(m_path)) {
    // New split generated! This means, the TRUE and FALSE branches are now
    // available and the generated decision must be set into the path.
    cnfTree.setStateFromLocal(m_path, CNFTree::Split);

    // Both paths are added to the factory, the rebalance mechanism takes care
    // of distribution to other compute nodes.
    {
      // LEFT
      CNFTree::Path p = CNFTree::getNextLeftPath(m_path);
      m_factory->addPath(p, TaskFactory::CubeOrSolve, m_originator);
    }
    {
      // RIGHT
      CNFTree::Path p = CNFTree::getNextRightPath(m_path);
      m_factory->addPath(p, TaskFactory::CubeOrSolve, m_originator);
    }

    return std::make_unique<TaskResult>(TaskResult::DecisionMade);
  } else {
    PARACUBER_LOG(*m_logger, Trace)
      << "No decision made for path " << CNFTree::pathToStrNoAlloc(m_path);

    m_factory->addPath(m_path, TaskFactory::Solve, m_originator);

    return std::make_unique<TaskResult>(TaskResult::NoDecisionMade);
  }
}
}
