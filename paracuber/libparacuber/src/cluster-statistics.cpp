#include "../include/paracuber/cluster-statistics.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/networked_node.hpp"
#include <algorithm>
#include <boost/iterator/filter_iterator.hpp>
#include <capnp-schemas/message.capnp.h>

namespace paracuber {

ClusterStatistics::Node::Node(bool& changed, int64_t id)
  : m_changed(changed)
  , m_acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                          20)
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
  , m_readyForWork(o.m_readyForWork)
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

ClusterStatistics::ClusterStatistics(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_logger(log->createLogger())
{}

ClusterStatistics::~ClusterStatistics() {}

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
  auto [it, inserted] = m_nodeMap.emplace(std::pair{ id, Node(m_changed, id) });
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
ClusterStatistics::offloadDecisionToRemote(int64_t originator)
{
  auto [map, lock] = getNodeMap();

  int64_t localId = m_config->getInt64(Config::Id);
  auto filterFunc = [originator, localId](auto& e) {
    auto& n = e.second;
    return n.getFullyKnown() && n.getReadyForWork() && n.getId() != localId &&
           n.getId() != originator && n.getDaemon();
  };

  // Filter to only contain nodes that can be worked with.
  auto filteredMap =
    boost::make_filter_iterator(filterFunc, map.begin(), map.end());

  auto& min = std::min(map.begin(), map.end(), [](auto& l, auto& r) {
    return l->second.getUtilization() < r->second.getUtilization();
  });
  if(min->second.getUtilization() <
     m_config->getFloat(Config::MaxNodeUtilization))
    return &min->second;
  return nullptr;
}

bool
ClusterStatistics::clearChanged()
{
  bool changed = m_changed;
  m_changed = false;
  return changed;
}
}
