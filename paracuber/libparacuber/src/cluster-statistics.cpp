#include "../include/paracuber/cluster-statistics.hpp"
#include "../include/paracuber/networked_node.hpp"

namespace paracuber {

ClusterStatistics::Node::Node()
  : acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                        20)
{}

ClusterStatistics::Node::Node(Node&& o) noexcept
  : name(std::move(o.name))
  , host(std::move(o.host))
  , networkedNode(std::move(o.networkedNode))
  , maximumCPUFrequency(o.maximumCPUFrequency)
  , uptime(o.uptime)
  , availableWorkers(o.availableWorkers)
  , workQueueCapacity(o.workQueueCapacity)
  , workQueueSize(o.workQueueSize)
  , id(o.id)
  , acc_workQueueSize(boost::accumulators::tag::rolling_window::window_size =
                        20)
{}
ClusterStatistics::Node::~Node() {}

ClusterStatistics::ClusterStatistics(ConfigPtr config, LogPtr log)
  : m_config(config)
{}

ClusterStatistics::~ClusterStatistics() {}

void
ClusterStatistics::addNode(Node&& node)
{
  m_nodeMap.emplace(node.name, std::move(node));
}
}
