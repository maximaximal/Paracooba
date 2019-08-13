#ifndef PARACUBER_CLUSTERSTATISTICS_HPP
#define PARACUBER_CLUSTERSTATISTICS_HPP

#include "log.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

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
  class Node
  {
    public:
    Node(int64_t id = 0);
    ~Node();

    Node(Node&& o) noexcept;

    void setName(const std::string& name) { m_name = name; }
    void setHost(const std::string& host) { m_host = host; }
    void setNetworkedNode(std::unique_ptr<NetworkedNode> networkedNode);
    void setMaximumCPUFrequency(uint16_t maximumCPUFrequency)
    {
      m_maximumCPUFrequency = maximumCPUFrequency;
    }
    void setAvailableWorkers(uint16_t availableWorkers)
    {
      m_availableWorkers = availableWorkers;
    }
    void setUptime(uint16_t uptime) { m_uptime = uptime; }
    void setWorkQueueCapacity(uint64_t workQueueCapacity)
    {
      m_workQueueCapacity = workQueueCapacity;
    }
    void setWorkQueueSize(uint64_t workQueueSize);
    void setId(int64_t id) { m_id = id; }
    void setFullyKnown(bool fullyKnown) { m_fullyKnown = fullyKnown; }
    void setUdpListenPort(uint16_t udpListenPort)
    {
      m_udpListenPort = udpListenPort;
    }

    NetworkedNode* getNetworkedNode() { return m_networkedNode.get(); }

    int64_t getId() { return m_id; }
    bool getFullyKnown() { return m_fullyKnown; }
    uint16_t getUdpListenPort() { return m_udpListenPort; }

    private:
    friend class ClusterStatistics;

    std::string m_name = "Unknown";
    std::string m_host = "";
    std::unique_ptr<NetworkedNode> m_networkedNode;

    uint16_t m_maximumCPUFrequency = 0;
    uint16_t m_availableWorkers = 0;
    uint16_t m_udpListenPort = 0;
    uint32_t m_uptime = 0;
    uint64_t m_workQueueCapacity = 0;
    uint64_t m_workQueueSize = 0;
    int64_t m_id = 0;
    bool m_fullyKnown = false;

    // Aggregating
    ::boost::accumulators::accumulator_set<
      uint64_t,
      ::boost::accumulators::stats<::boost::accumulators::tag::rolling_mean>>
      m_acc_workQueueSize;
  };

  /** @brief Constructor
   */
  ClusterStatistics(ConfigPtr config, LogPtr log);
  /** @brief Destructor.
   */
  ~ClusterStatistics();

  Node& getOrCreateNode(int64_t id);

  protected:
  friend class Communicator;
  void addNode(Node&& node);
  void removeNode(int64_t id);

  private:
  using NodeMap = std::unordered_map<int64_t, Node>;
  NodeMap m_nodeMap;

  ConfigPtr m_config;
  Logger m_logger;
};
}

#endif
