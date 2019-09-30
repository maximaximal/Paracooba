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

  CNFTree::State state;
  assert(cnfTree.getState(m_path, state));
  PARACUBER_LOG(*m_logger, Trace)
    << "State for path " << CNFTree::pathToStrNoAlloc(m_path) << ":" << state;
  assert(state == CNFTree::Unvisited);

  m_rootCNF->getCNFTree().setState(m_path, CNFTree::Working);

  CNFTree::CubeVar var = 0;
  if(m_rootCNF->getCuberRegistry().generateCube(m_path, var)) {
    // New cube generated! This means, the TRUE and FALSE branches are now
    // available and the generated decision must be set into the path.
    assert(m_rootCNF->getCNFTree().setDecisionAndState(m_path, var, CNFTree::Split));

    {
      // LEFT
      CNFTree::Path p = CNFTree::getNextLeftPath(m_path);
      cnfTree.setDecisionAndState(p, 0, CNFTree::Unvisited);

      const ClusterStatistics::Node* target =
        clusterStatistics->getTargetComputeNodeForNewDecision(m_originator);

      if(!target) {
        m_factory->addPath(p, TaskFactory::CubeOrSolve, m_originator);
      } else {
        clusterStatistics->handlePathOnNode(target, m_rootCNF, p);
      }
    }
    {
      // RIGHT
      CNFTree::Path p = CNFTree::getNextRightPath(m_path);
      cnfTree.setDecisionAndState(p, 0, CNFTree::Unvisited);

      const ClusterStatistics::Node* target =
        clusterStatistics->getTargetComputeNodeForNewDecision(m_originator);
      if(!target) {
        m_factory->addPath(p, TaskFactory::CubeOrSolve, m_originator);
      } else {
        clusterStatistics->handlePathOnNode(target, m_rootCNF, p);
      }
    }

    return std::make_unique<TaskResult>(TaskResult::DecisionMade);
  } else {
    // This is an invalid leaf without a decision! Only one level above can a
    // solver be created.
    PARACUBER_LOG(*m_logger, Trace) << "No decision made!";
    cnfTree.setState(m_path, CNFTree::Dropped);
    return std::make_unique<TaskResult>(TaskResult::NoDecisionMade);
  }
}

void
DecisionTask::terminate()
{}
}
