#ifndef PARACUBER_CLUSTERSTATISTICS_HPP
#define PARACUBER_CLUSTERSTATISTICS_HPP

#include "cnftree.hpp"
#include "log.hpp"
#include "util.hpp"
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

namespace paracuber {

#define CLUSTERSTATISTICS_NODE_CHANGED(MEMBER, VAR) \
  if(MEMBER != VAR) {                               \
    MEMBER = VAR;                                   \
    m_changed = true;                               \
  }

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
    explicit Node(bool& changed, int64_t id = 0);
    ~Node();

    Node(Node&& o) noexcept;

    void setName(const std::string& name)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_name, name)
    }
    void setName(std::string_view name)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_name, name)
    }
    void setHost(const std::string& host)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_host, host)
    }
    void setNetworkedNode(std::unique_ptr<NetworkedNode> networkedNode);
    void setMaximumCPUFrequency(uint16_t maximumCPUFrequency)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_maximumCPUFrequency, maximumCPUFrequency)
    }
    void setAvailableWorkers(uint16_t availableWorkers)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_availableWorkers, availableWorkers);
    }
    void setUptime(uint16_t uptime)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_uptime, uptime)
    }
    void setWorkQueueCapacity(uint64_t workQueueCapacity)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_workQueueCapacity, workQueueCapacity)
    }
    void setWorkQueueSize(uint64_t workQueueSize);
    void setId(int64_t id) { CLUSTERSTATISTICS_NODE_CHANGED(m_id, id) }
    void setFullyKnown(bool fullyKnown)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_fullyKnown, fullyKnown)
    }
    void setUdpListenPort(uint16_t udpListenPort)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_udpListenPort, udpListenPort)
    }
    void setTcpListenPort(uint16_t tcpListenPort)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_tcpListenPort, tcpListenPort)
    }
    void setReadyForWork(bool ready)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_readyForWork, ready)
    }
    void setDaemon(bool daemon)
    {
      CLUSTERSTATISTICS_NODE_CHANGED(m_daemon, daemon)
    }
    void setDistance(uint8_t distance){
      CLUSTERSTATISTICS_NODE_CHANGED(m_distance, distance)
    }

    NetworkedNode* getNetworkedNode() const
    {
      return m_networkedNode.get();
    }

    std::string_view getName() const { return m_name; }
    int64_t getId() const { return m_id; }
    bool getFullyKnown() const { return m_fullyKnown; }
    bool getReadyForWork() const { return m_readyForWork; }
    bool getDaemon() const { return m_daemon; }
    uint16_t getUdpListenPort() const { return m_udpListenPort; }
    uint16_t getTcpListenPort() const { return m_tcpListenPort; }
    uint64_t getWorkQueueSize() const { return m_workQueueSize; }
    float getUtilization() const
    {
      return static_cast<float>(m_workQueueSize) /
             static_cast<float>(m_workQueueCapacity);
    }
    float getFitnessForNewAssignment() const
    {
      // This addition determines the weight local decisions have. A greater
      // value makes local decisions more unlikely.
      return getUtilization() * (m_distance + 0.8);
    }

    private:
    friend class ClusterStatistics;

    std::string m_name = "Unknown";
    std::string m_host = "";
    std::unique_ptr<NetworkedNode> m_networkedNode;

    uint16_t m_maximumCPUFrequency = 0;
    uint16_t m_availableWorkers = 0;
    uint16_t m_udpListenPort = 0;
    uint16_t m_tcpListenPort = 0;
    uint32_t m_uptime = 0;
    uint64_t m_workQueueCapacity = 0;
    uint64_t m_workQueueSize = 0;
    int64_t m_id = 0;
    bool m_fullyKnown = false;
    bool m_readyForWork = false;
    bool m_daemon = false;
    uint8_t m_distance = 0;

    bool& m_changed;

    // Aggregating
    ::boost::accumulators::accumulator_set<
      uint64_t,
      ::boost::accumulators::stats<::boost::accumulators::tag::rolling_mean>>
      m_acc_workQueueSize;

    bool operator=(const Node& n) { return m_id == n.m_id; }

    friend std::ostream& operator<<(std::ostream& o, const Node& n)
    {
      o << "\"name\": \"" << n.m_name << "\","
        << "\"id\": " << n.m_id << ","
        << "\"maximumCPUFrequency\": " << n.m_maximumCPUFrequency << ","
        << "\"availableWorkers\": " << n.m_availableWorkers << ","
        << "\"udpListenPort\": " << n.m_udpListenPort << ","
        << "\"uptime\": " << n.m_uptime << ","
        << "\"workQueueCapacity\": " << n.m_workQueueCapacity << ","
        << "\"readyForWork\": " << n.m_readyForWork << ","
        << "\"fullyKnown\": " << n.m_fullyKnown << ","
        << "\"daemon\": " << n.m_daemon << ","
        << "\"workQueueSize\": " << n.m_workQueueSize;
      return o;
    }
  };

  /** @brief Constructor
   */
  ClusterStatistics(ConfigPtr config, LogPtr log);
  /** @brief Destructor.
   */
  ~ClusterStatistics();

  void initLocalNode();

  ClusterStatistics::Node& getNode(int64_t id);

  std::pair<ClusterStatistics::Node&, bool> getOrCreateNode(int64_t id);

  friend std::ostream& operator<<(std::ostream& o, const ClusterStatistics& c)
  {
    bool first = true;
    o << "{ \"type\": \"clusterstatistics\", \"ClusterStatistics\": [";
    for(auto& it : c.m_nodeMap) {
      auto& node = it.second;
      o << "{" << node << "}";
      if(first && c.m_nodeMap.size() > 1) {
        o << ",";
        first = false;
      }
    }
    o << "]}";
    return o;
  }

  using NodeMap = std::map<int64_t, Node>;

  ConstSharedLockView<NodeMap> getNodeMap();

  /** @brief Determine if the next decision should be offloaded to another
   * compute node.
   *
   * @returns nullptr if the decision is better done locally, remote node
   * otherwise.
   */
  const Node* getTargetComputeNodeForNewDecision(int64_t originator);
  bool clearChanged();

  const Node& getThisNode() const { return *m_thisNode; }

  void handlePathOnNode(const Node* node,
                        std::shared_ptr<CNF> rootCNF,
                        CNFTree::Path p);

  protected:
  friend class Communicator;
  void addNode(Node&& node);
  void removeNode(int64_t id);

  private:
  NodeMap m_nodeMap;
  Node* m_thisNode;
  bool m_changed = false;

  ConfigPtr m_config;
  Logger m_logger;
  std::shared_mutex m_nodeMapMutex;
};
}

#endif
