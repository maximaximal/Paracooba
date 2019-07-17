#ifndef PARACUBER_CLUSTERSTATISTICS_HPP
#define PARACUBER_CLUSTERSTATISTICS_HPP

#include "log.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

namespace paracuber {
class NetworkedNode;

/** @brief Statistics about the whole cluster, based on which decisions may be
 * made.
 */
class ClusterStatistics
{
  public:
  /** @brief Statistics about the performance of a node in the cluster.
   *
   * These statistics are gathered for every node.
   */
  struct Node
  {
    Node();
    ~Node();

    Node(Node&& o) noexcept;

    std::string name = "Unknown";
    std::string host = "";
    std::unique_ptr<NetworkedNode> networkedNode;

    uint16_t maximumCPUFrequency = 0;
    uint16_t availableWorkers = 0;
    uint32_t uptime = 0;
    uint64_t workQueueCapacity = 0;
    uint64_t workQueueSize = 0;
    int64_t id = 0;

    // Aggregating
    boost::accumulators::accumulator_set<
      uint64_t,
      boost::accumulators::stats<boost::accumulators::tag::rolling_mean>>
      acc_workQueueSize;
  };

  /** @brief Constructor
   */
  ClusterStatistics(ConfigPtr config, LogPtr log);
  /** @brief Destructor.
   */
  ~ClusterStatistics();

  protected:
  friend class Communicator;
  void addNode(Node&& node);

  private:
  using NodeMap = std::map<std::string, Node>;
  NodeMap m_nodeMap;

  ConfigPtr m_config;
};
}

#endif
