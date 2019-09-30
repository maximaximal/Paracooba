#include "../include/paracuber/cluster-statistics.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/daemon.hpp"
#include "../include/paracuber/networked_node.hpp"
#include <algorithm>
#include <boost/iterator/filter_iterator.hpp>
#include <capnp-schemas/message.capnp.h>

namespace paracuber {

ClusterStatistics::Node::Node(bool& changed, int64_t thisId, int64_t id)
  : m_changed(changed)
  , m_acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                          20)
  , m_thisId(thisId)
  , m_id(id)
{}

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
  , m_contextStates(std::move(o.m_contextStates))
  , m_thisId(o.m_thisId)
  , m_acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                          20)
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

ClusterStatistics::ClusterStatistics(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_logger(log->createLogger())
{}

ClusterStatistics::~ClusterStatistics() {}

void
ClusterStatistics::initLocalNode()
{
  Node thisNode(
    m_changed, m_config->getInt64(Config::Id), m_config->getInt64(Config::Id));
  thisNode.setDaemon(m_config->isDaemonMode());
  thisNode.setDistance(0);
  thisNode.setFullyKnown(true);
  thisNode.setUptime(0);
  thisNode.setWorkQueueCapacity(m_config->getUint64(Config::WorkQueueCapacity));
  thisNode.setWorkQueueSize(0);
  thisNode.setTcpListenPort(m_config->getUint16(Config::TCPListenPort));
  thisNode.setUdpListenPort(m_config->getUint16(Config::UDPListenPort));
  thisNode.setName(m_config->getString(Config::LocalName));
  addNode(std::move(thisNode));
  m_thisNode = &getNode(m_config->getInt64(Config::Id));
}

ClusterStatistics::Node&
ClusterStatistics::getNode(int64_t id)
{
  auto it = m_nodeMap.find(id);
  assert(it != m_nodeMap.end());
  return it->second;
}

std::pair<ClusterStatistics::Node&, bool>
ClusterStatistics::getOrCreateNode(int64_t id)
{
  auto [it, inserted] = m_nodeMap.emplace(
    std::pair{ id, Node(m_changed, m_thisNode->getId(), id) });
  return { it->second, inserted };
}

void
ClusterStatistics::addNode(Node&& node)
{
  m_nodeMap.emplace(node.m_id, std::move(node));
}
void
ClusterStatistics::removeNode(int64_t id)
{
  m_nodeMap.erase(id);
}

ConstSharedLockView<ClusterStatistics::NodeMap>
ClusterStatistics::getNodeMap()
{
  std::shared_lock lock(m_nodeMapMutex);
  return { m_nodeMap, std::move(lock) };
}

const ClusterStatistics::Node*
ClusterStatistics::getTargetComputeNodeForNewDecision(int64_t originator)
{

  auto [map, lock] = getNodeMap();

  int64_t localId = m_config->getInt64(Config::Id);
  auto filterFunc = [originator, localId](auto& e) {
    auto& n = e.second;
    return n.getFullyKnown() && n.getReadyForWork() &&
           n.getId() != originator && n.getDaemon();
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
  if(target == m_thisNode) {
    return nullptr;
  }

  return target;
}

void
ClusterStatistics::handlePathOnNode(const Node* node,
                                    std::shared_ptr<CNF> rootCNF,
                                    CNFTree::Path p)
{
  assert(node);

  Communicator* comm = m_config->getCommunicator();

  // Local node should be handled externally, without using this function.
  assert(node != m_thisNode);

  // This path should be handled on another compute node. This means, the
  // other compute node requires a Cube-Beam from the Communicator class.
  m_config->getCommunicator()->sendCNFToNode(
    rootCNF, p, node->getNetworkedNode());
}

bool
ClusterStatistics::clearChanged()
{
  bool changed = m_changed;
  m_changed = false;
  return changed;
}
}
