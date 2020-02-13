#include "../include/paracuber/cluster-statistics.hpp"
#include "../include/paracuber/client.hpp"
#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/networked_node.hpp"
#include "../include/paracuber/task.hpp"
#include "../include/paracuber/task_factory.hpp"
#include <algorithm>
#include <boost/iterator/filter_iterator.hpp>
#include <string>

namespace paracuber {

static const size_t ClusterStatisticsNodeWindowSize = 20;

ClusterStatistics::Node::Node(bool& changed, int64_t thisId, int64_t id)
  : m_changed(changed)
  , m_acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                          ClusterStatisticsNodeWindowSize)
  , m_acc_durationSinceLastStatus(
      boost::accumulators::tag::rolling_window::window_size =
        ClusterStatisticsNodeWindowSize)
  , m_thisId(thisId)
  , m_id(id)
{
  initMeanDuration(ClusterStatisticsNodeWindowSize);
}

ClusterStatistics::Node::Node(Node&& o) noexcept
  : m_changed(o.m_changed)
  , m_name(std::move(o.m_name))
  , m_host(std::move(o.m_host))
  , m_networkedNode(std::move(o.m_networkedNode))
  , m_maximumCPUFrequency(o.m_maximumCPUFrequency)
  , m_uptime(o.m_uptime)
  , m_availableWorkers(o.m_availableWorkers)
  , m_workQueueCapacity(o.m_workQueueCapacity)
  , m_workQueueSize(o.m_workQueueSize)
  , m_id(o.m_id)
  , m_fullyKnown(o.m_fullyKnown)
  , m_daemon(o.m_daemon)
  , m_distance(o.m_distance)
  , m_contexts(std::move(o.m_contexts))
  , m_thisId(o.m_thisId)
  , m_acc_workQueueSize(std::move(o.m_acc_workQueueSize))
  , m_acc_durationSinceLastStatus(std::move(o.m_acc_durationSinceLastStatus))
{}
ClusterStatistics::Node::~Node() {}

void
ClusterStatistics::Node::setNetworkedNode(
  std::unique_ptr<NetworkedNode> networkedNode)
{
  CLUSTERSTATISTICS_NODE_CHANGED(m_networkedNode, std::move(networkedNode))
}

void
ClusterStatistics::Node::setWorkQueueSize(uint64_t workQueueSize)
{
  CLUSTERSTATISTICS_NODE_CHANGED(m_workQueueSize, workQueueSize)
  m_acc_workQueueSize(workQueueSize);
}
bool
ClusterStatistics::Node::getReadyForWork(int64_t id) const
{
  if(id == 0)
    id = m_thisId;
  Daemon::Context::State state =
    static_cast<Daemon::Context::State>(getContextState(id));
  return state & Daemon::Context::WaitingForWork;
}
void
ClusterStatistics::Node::applyTaskFactoryVector(const TaskFactoryVector& v)
{
  for(auto& e : v) {
    setContextSize(e->getOriginId(), e->getSize());
  }
}

ClusterStatistics::ClusterStatistics(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_logger(log->createLogger("ClusterStatistics"))
{}

ClusterStatistics::~ClusterStatistics() {}

void
ClusterStatistics::initLocalNode()
{
  Node thisNode(
    m_changed, m_config->getInt64(Config::Id), m_config->getInt64(Config::Id));
  thisNode.setDaemon(m_config->isDaemonMode());
  thisNode.setDistance(1);
  thisNode.setFullyKnown(true);
  thisNode.setUptime(0);
  thisNode.setWorkQueueCapacity(m_config->getUint64(Config::WorkQueueCapacity));
  thisNode.setWorkQueueSize(0);
  thisNode.setTcpListenPort(m_config->getUint16(Config::TCPListenPort));
  thisNode.setUdpListenPort(m_config->getUint16(Config::UDPListenPort));
  thisNode.setName(m_config->getString(Config::LocalName));
  m_thisNode = &addNode(std::move(thisNode));
}

const ClusterStatistics::Node&
ClusterStatistics::getNode(int64_t id) const
{
  auto [map, lock] = getNodeMap();
  auto it = map.find(id);
  assert(it != map.end());
  return it->second;
}

ClusterStatistics::Node&
ClusterStatistics::getNode(int64_t id)
{
  auto [map, lock] = getNodeMap();
  auto it = map.find(id);
  assert(it != map.end());
  return it->second;
}

std::pair<ClusterStatistics::Node&, bool>
ClusterStatistics::getOrCreateNode(int64_t id)
{
  auto [it, inserted] = m_nodeMap.emplace(
    std::pair{ id, Node(m_changed, m_thisNode->getId(), id) });
  return { it->second, inserted };
}

ClusterStatistics::Node&
ClusterStatistics::addNode(Node&& node)
{
  auto [map, lock] = getUniqueNodeMap();
  return map.emplace(node.m_id, std::move(node)).first->second;
}

void
ClusterStatistics::removeNode(int64_t id, const std::string& reason)
{
  auto [map, lock] = getUniqueNodeMap();
  unsafeRemoveNode(id, reason);
}

void
ClusterStatistics::unsafeRemoveNode(int64_t id, const std::string& reason)
{
  assert(m_thisNode);
  if(id == m_thisNode->m_id)
    return;

  auto it = m_nodeMap.find(id);
  if(it != m_nodeMap.end()) {
    PARACUBER_LOG(m_logger, Trace)
      << "Remove cluster statistics node with id: " << id
      << " becase of reason: " << reason;

    it->second.m_nodeOfflineSignal(reason);
    m_nodeMap.erase(id);
  }
}

SharedLockView<ClusterStatistics::NodeMap&>
ClusterStatistics::getNodeMap()
{
  std::shared_lock lock(m_nodeMapMutex);
  return { m_nodeMap, std::move(lock) };
}
ConstSharedLockView<ClusterStatistics::NodeMap>
ClusterStatistics::getNodeMap() const
{
  std::shared_lock lock(m_nodeMapMutex);
  return { m_nodeMap, std::move(lock) };
}

UniqueLockView<ClusterStatistics::NodeMap&>
ClusterStatistics::getUniqueNodeMap()
{
  std::unique_lock lock(m_nodeMapMutex);
  return { m_nodeMap, std::move(lock) };
}

ClusterStatistics::Node*
ClusterStatistics::getTargetComputeNodeForNewDecision(CNFTree::Path p,
                                                      int64_t originator)
{
  if(CNFTree::getDepth(p) < 1) {
    return nullptr;
  }

  return getFittestNodeForNewWork(originator);
}

ClusterStatistics::Node*
ClusterStatistics::getFittestNodeForNewWork(int originator)
{
  auto [map, lock] = getNodeMap();

  int64_t localId = m_config->getInt64(Config::Id);
  auto filterFunc = [originator, localId](auto& e) {
    auto& n = e.second;
    return n.getFullyKnown() && n.getReadyForWork() && n.getId() != localId;
  };

  // Filter to only contain nodes that can be worked with.
  auto filteredMap =
    boost::make_filter_iterator(filterFunc, map.begin(), map.end());
  auto filteredMapEnd =
    boost::make_filter_iterator(filterFunc, map.end(), map.end());

  if(filteredMap == filteredMapEnd) {
    return nullptr;
  }

  auto& min = std::min(filteredMap, filteredMapEnd, [](auto& l, auto& r) {
    return l->second.getFitnessForNewAssignment() <
           r->second.getFitnessForNewAssignment();
  });

  auto target = &min->second;

  // No limit for max node utilisation! Just send the node to the best fitting
  // place.
  return target;
}

void
ClusterStatistics::handlePathOnNode(int64_t originator,
                                    Node& node,
                                    std::shared_ptr<CNF> rootCNF,
                                    CNFTree::Path p)
{
  Communicator* comm = m_config->getCommunicator();

  // Local node should be handled externally, without using this function.
  assert(&node != m_thisNode);

  TaskFactory* taskFactory = rootCNF->getTaskFactory();
  assert(taskFactory);
  taskFactory->addExternallyProcessingTask(originator, p, node);

  // This path should be handled on another compute node. This means, the
  // other compute node requires a Cube-Beam from the Communicator class.
  rootCNF->sendPath(node.getNetworkedNode(), p, []() {});
}

bool
ClusterStatistics::clearChanged()
{
  bool changed = m_changed;
  m_changed = false;
  return changed;
}

void
ClusterStatistics::rebalance(int originator, TaskFactory& factory)
{
  // Rebalancing must be done once for every context.
  auto mostFitNode = getFittestNodeForNewWork(originator);
  if(mostFitNode && !mostFitNode->isFullyUtilized() &&
     factory.canProduceTask()) {
    PARACUBER_LOG(m_logger, Trace)
      << "Rebalance " << mostFitNode->getSlotsLeft() + 1
      << " tasks for work with origin " << originator << " to node "
      << mostFitNode->getName() << " (" << mostFitNode->getId() << ")";

    // Send as much work over there as that node has space left, but always at
    // least 1.
    for(size_t i = 0;
        i < mostFitNode->getSlotsLeft() + 1 && factory.canProduceTask();
        ++i) {
      auto skel = factory.produceTaskSkeleton();
      handlePathOnNode(originator, *mostFitNode, factory.getRootCNF(), skel.p);
    }
  }
}
void
ClusterStatistics::rebalance()
{
  if(m_config->hasDaemon()) {
    auto [contextMap, lock] = m_config->getDaemon()->getContextMap();
    for(auto& ctx : contextMap) {
      rebalance(ctx.second->getOriginatorId(), *ctx.second->getTaskFactory());
    }
  } else {
    rebalance(m_config->getInt64(Config::Id),
              *m_config->getClient()->getTaskFactory());
  }
}
void
ClusterStatistics::tick()
{
  auto [map, lock] = getUniqueNodeMap();

  m_thisNode->statusReceived();

  for(auto& it : map) {
    auto& statNode = it.second;
    auto lastStatus = statNode.getDurationSinceLastStatus();
    auto mean = statNode.getMeanDurationSinceLastStatus();
    if(lastStatus > mean * 3) {
      std::string message = "Last status update was too long ago.";
      unsafeRemoveNode(it.first, message);
    }
  }
}
}
