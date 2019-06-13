#ifndef PARACUBER_CLUSTERSTATISTICS_HPP
#define PARACUBER_CLUSTERSTATISTICS_HPP

#include <map>
#include <string>
#include <cstdint>

namespace paracuber {
class ClusterStatistics
{
public:
  /** @brief Statistics about the performance of a node in the cluster.
   *
   * These statistics are gathered for every node.
   */
  struct Node {
    std::string_view name = "Unknown";
    uint32_t cpu_score = 0;
    uint32_t queue_length = 0;
  };

  ClusterStatistics();
  ~ClusterStatistics();

private:
  using NodeMap = std::map<std::string, Node>();
  NodeMap m_nodeMap;
};
}

#endif
