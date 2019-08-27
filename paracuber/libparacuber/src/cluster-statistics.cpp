#include "../include/paracuber/cluster-statistics.hpp"
#include "../include/paracuber/networked_node.hpp"
#include <capnp-schemas/message.capnp.h>

namespace paracuber {

ClusterStatistics::Node::Node(int64_t id)
  : m_acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                          20)
  , m_id(id)
{}

ClusterStatistics::Node::Node(Node&& o) noexcept
  : m_name(std::move(o.m_name))
  , m_host(std::move(o.m_host))
  , m_networkedNode(std::move(o.m_networkedNode))
  , m_maximumCPUFrequency(o.m_maximumCPUFrequency)
  , m_uptime(o.m_uptime)
  , m_availableWorkers(o.m_availableWorkers)
  , m_workQueueCapacity(o.m_workQueueCapacity)
  , m_workQueueSize(o.m_workQueueSize)
  , m_id(o.m_id)
  , m_fullyKnown(o.m_fullyKnown)
  , m_acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                          20)
{}
ClusterStatistics::Node::~Node() {}

void
ClusterStatistics::Node::setNetworkedNode(
  std::unique_ptr<NetworkedNode> networkedNode)
{
  m_networkedNode = std::move(networkedNode);
}

void
ClusterStatistics::Node::setWorkQueueSize(uint64_t workQueueSize)
{
  m_workQueueSize = workQueueSize;
  m_acc_workQueueSize(workQueueSize);
}

ClusterStatistics::ClusterStatistics(ConfigPtr config, LogPtr log)
  : m_config(config)
  , m_logger(log->createLogger())
{}

ClusterStatistics::~ClusterStatistics() {}

std::pair<ClusterStatistics::Node&, bool>
ClusterStatistics::getOrCreateNode(int64_t id)
{
  auto [it, inserted] = m_nodeMap.emplace(std::make_pair(id, Node(id)));
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
}
