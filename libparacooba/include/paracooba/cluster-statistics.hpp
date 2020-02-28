#ifndef PARACOOBA_CLUSTERSTATISTICS_HPP
#define PARACOOBA_CLUSTERSTATISTICS_HPP

#include "cnftree.hpp"
#include "log.hpp"
#include "util.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/signals2/signal.hpp>

namespace paracooba {

#define CLUSTERSTATISTICS_NODE_CHANGED(MEMBER, VAR) \
  if(MEMBER != VAR) {                               \
    MEMBER = VAR;                                   \
    m_changed = true;                               \
  }

class NetworkedNode;
class TaskFactory;

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
    explicit Node(bool& changed, int64_t thisId, int64_t id);
    ~Node();

    Node(Node&& o) noexcept;

    struct Context
    {
      uint8_t state = 0;
      uint64_t queueSize = 0;
    };

    using ContextMap = std::map<int64_t, Context>;

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
    /** @brief Get ready for work state.
     *
     * Depends on client or daemon mode. In client mode, this returns the
     * readyness for the client ID. In daemon mode, returns the state for the
     * specified ID.*/
    bool getReadyForWork(int64_t id = 0) const;
    bool isDaemon() const { return m_daemon; }
    uint16_t getUdpListenPort() const { return m_udpListenPort; }
    uint16_t getTcpListenPort() const { return m_tcpListenPort; }
    uint64_t getWorkQueueSize() const { return m_workQueueSize; }
    float getUtilization() const
    {
      return static_cast<float>(m_workQueueSize) /
             static_cast<float>(m_availableWorkers);
    }
    /** @brief Calculate fitness for new assigned work. Lower means fitter.
     */
    float getFitnessForNewAssignment() const
    {
      // TODO: Ping should be considered.
      return getUtilization();
    }
    UniqueLockView<ContextMap&> getContextMap()
    {
      return { m_contexts, std::move(std::unique_lock(m_contextsMutex)) };
    }
    ConstSharedLockView<ContextMap> getContextMapShared() const
    {
      return { m_contexts, std::move(std::shared_lock(m_contextsMutex)) };
    }
    uint8_t getContextState(int64_t id) const
    {
      auto [map, lock] = getContextMapShared();
      auto it = map.find(id);
      if(it == map.end())
        return 0;
      return it->second.state;
    }
    void setContextState(int64_t id, uint8_t state)
    {
      auto [map, lock] = getContextMap();
      CLUSTERSTATISTICS_NODE_CHANGED(map[id].state, state)
    }
    void setContextSize(int64_t id, uint64_t size)
    {
      auto [map, lock] = getContextMap();
      CLUSTERSTATISTICS_NODE_CHANGED(map[id].queueSize, size)
    }
    void setContextStateAndSize(int64_t id, uint8_t state, uint64_t size)
    {
      auto [map, lock] = getContextMap();
      CLUSTERSTATISTICS_NODE_CHANGED(map[id].state, state)
      CLUSTERSTATISTICS_NODE_CHANGED(map[id].queueSize, size)
    }
    uint64_t getAggregatedContextSize() const
    {
      uint64_t size = 0;
      auto [map, lock] = getContextMapShared();
      for(auto& e : map) {
        size += e.second.queueSize;
      }
      return size;
    }

    bool isFullyUtilized() const
    {
      return ((float)m_workQueueSize / (float)m_availableWorkers) > 1.5;
    }

    size_t getSlotsLeft() const
    {
      return static_cast<uint64_t>(m_availableWorkers) -
             std::min(m_workQueueSize,
                      static_cast<uint64_t>(m_availableWorkers));
    }
    uint16_t getAvailableWorkers() const { return m_availableWorkers; }

    void statusReceived()
    {
      auto now = std::chrono::system_clock::now();
      auto durationSinceLastStatus = now - m_lastStatusReceived;
      m_lastStatusReceived = now;
      m_acc_durationSinceLastStatus(durationSinceLastStatus);
    }

    std::chrono::duration<double> getDurationSinceLastStatus() const
    {
      auto now = std::chrono::system_clock::now();
      return now - m_lastStatusReceived;
    }

    std::chrono::duration<double> getMeanDurationSinceLastStatus() const
    {
      return ::boost::accumulators::rolling_mean(m_acc_durationSinceLastStatus);
    }

    bool statusIsOverdue() const
    {
      return getDurationSinceLastStatus() >
             getMeanDurationSinceLastStatus() * 10;
    }

    void initMeanDuration(size_t windowSize)
    {
      using namespace std::chrono_literals;
      for(size_t i = 0; i < windowSize; ++i) {
        m_acc_durationSinceLastStatus(5s);
      }
    }

    using TaskFactoryVector = std::vector<TaskFactory*>;
    void applyTaskFactoryVector(const TaskFactoryVector& v);

    using NodeOfflineSignal = boost::signals2::signal<void(const std::string&)>;
    NodeOfflineSignal& getNodeOfflineSignal() { return m_nodeOfflineSignal; }

    private:
    friend class ClusterStatistics;

    std::string m_name = "Unknown";
    std::string m_host = "";
    std::unique_ptr<NetworkedNode> m_networkedNode;

    std::chrono::time_point<std::chrono::system_clock> m_lastStatusReceived =
      std::chrono::system_clock::now();

    uint16_t m_maximumCPUFrequency = 0;
    uint16_t m_availableWorkers = 0;
    uint16_t m_udpListenPort = 0;
    uint16_t m_tcpListenPort = 0;
    uint32_t m_uptime = 0;
    uint64_t m_workQueueCapacity = 0;
    uint64_t m_workQueueSize = 0;
    int64_t m_id = 0;
    int64_t m_thisId = 0;
    bool m_fullyKnown = false;
    bool m_daemon = false;
    uint8_t m_distance = 2;

    NodeOfflineSignal m_nodeOfflineSignal;

    mutable std::shared_mutex m_contextsMutex;
    ContextMap m_contexts;

    bool& m_changed;

    // Aggregating
    ::boost::accumulators::accumulator_set<
      uint64_t,
      ::boost::accumulators::stats<::boost::accumulators::tag::rolling_mean>>
      m_acc_workQueueSize;

    ::boost::accumulators::accumulator_set<
      std::chrono::duration<double>,
      ::boost::accumulators::stats<::boost::accumulators::tag::rolling_mean>>
      m_acc_durationSinceLastStatus;

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
        << "\"readyForWork\": " << n.getReadyForWork() << ","
        << "\"fullyKnown\": " << n.m_fullyKnown << ","
        << "\"daemon\": " << n.m_daemon << ","
        << "\"highlighted\": false,"
        << "\"fitnessForNewAssignment\": " << n.getFitnessForNewAssignment()
        << ","
        << "\"aggregatedContextSize\": " << n.getAggregatedContextSize() << ","
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

  using HandledNodesSet = std::set<Node*>;

  const ClusterStatistics::Node& getNode(int64_t id) const;
  ClusterStatistics::Node& getNode(int64_t id);

  std::pair<ClusterStatistics::Node&, bool> getOrCreateNode(int64_t id);

  friend std::ostream& operator<<(std::ostream& o, const ClusterStatistics& c)
  {
    bool first = true;
    o << "{ \"type\": \"clusterstatistics\", \"ClusterStatistics\": [";
    for(auto& it : c.m_nodeMap) {
      auto& node = it.second;
      if(first) {
        o << "{" << node << "}";
        first = false;
      } else {
        o << ",{" << node << "}";
      }
    }
    o << "]}";
    return o;
  }

  using NodeMap = std::map<int64_t, Node>;

  SharedLockView<NodeMap&> getNodeMap();
  ConstSharedLockView<NodeMap> getNodeMap() const;
  UniqueLockView<NodeMap&> getUniqueNodeMap();

  bool clearChanged();

  Node& getThisNode() { return *m_thisNode; }

  void handlePathOnNode(int64_t originator,
                        Node& node,
                        std::shared_ptr<CNF> rootCNF,
                        CNFTree::Path p);

  bool hasNode(int64_t id);

  Node* getFittestNodeForNewWork(int originator,
                                 const HandledNodesSet& handledNodes,
                                 int64_t rootCNFIDID = 0);

  /** @brief Start rebalancing of work to other nodes.
   *
   * Must be done once for every context on a daemon.
   *
   * Part of @see Rebalancing.
   */
  void rebalance(int originator, TaskFactory& factory);
  /** @brief Start rebalancing of work to other nodes.
   *
   * Automatically calls rebalance(int) with correct context.
   *
   * Part of @see Rebalancing.
   */
  void rebalance();

  /** @brief Check all connections for their timeouts. */
  void tick();

  protected:
  friend class Communicator;
  ClusterStatistics::Node& addNode(Node&& node);
  void removeNode(int64_t id, const std::string& reason);

  private:
  NodeMap m_nodeMap;
  Node* m_thisNode;
  bool m_changed = false;

  void unsafeRemoveNode(int64_t id, const std::string& reason);

  ConfigPtr m_config;
  Logger m_logger;
  mutable std::shared_mutex m_nodeMapMutex;
};
}

#endif
