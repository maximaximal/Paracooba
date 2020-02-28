#ifndef PARACOOBA_MESSAGES_NODE
#define PARACOOBA_MESSAGES_NODE

#include <cstdint>
#include <string>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace paracooba {
class NetworkedNode;

namespace messages {
class Node
{
  public:
  Node() {}
  Node(const std::string& name,
       int64_t id,
       uint32_t availableWorkers,
       uint64_t workQueueCapacity,
       uint64_t workQueueSize,
       uint64_t maximumCPUFrequency,
       uint32_t uptime,
       uint16_t udpListenPort,
       uint16_t tcpListenPort,
       bool daemonMode)
    : name(name)
    , id(id)
    , availableWorkers(availableWorkers)
    , workQueueCapacity(workQueueCapacity)
    , workQueueSize(workQueueSize)
    , maximumCPUFrequency(maximumCPUFrequency)
    , uptime(uptime)
    , udpListenPort(udpListenPort)
    , tcpListenPort(tcpListenPort)
    , daemonMode(daemonMode)
  {}
  virtual ~Node() {}

  struct KnownPeer
  {
    uint64_t ipAddress[2];
    uint16_t port;
    int64_t id;

    template<class Archive>
    void serialize(Archive& ar)
    {
      ar(ipAddress[0], ipAddress[1], CEREAL_NVP(port), CEREAL_NVP(id));
    }
  };
  using KnownPeersVector = std::vector<KnownPeer>;

  const std::string& getName() const { return name; };
  int64_t getId() const { return id; }
  uint32_t getAvailableWorkers() const { return availableWorkers; }
  uint64_t getWorkQueueCapacity() const { return workQueueCapacity; }
  uint64_t getWorkQueueSize() const { return workQueueSize; }
  uint64_t getMaximumCPUFrequency() const { return maximumCPUFrequency; }
  uint32_t getUptime() const { return uptime; }
  uint16_t getUdpListenPort() const { return udpListenPort; }
  uint16_t getTcpListenPort() const { return tcpListenPort; }
  bool getDaemonMode() const { return daemonMode; }
  const KnownPeersVector& getKnownPeers() const { return knownPeers; }

  void addKnownPeer(uint32_t ipAddress, uint16_t port, int64_t id)
  {
    knownPeers.push_back(KnownPeer{ { 0, ipAddress }, port, id });
  }
  void addKnownPeer(uint64_t ipAddress[2], uint16_t port, int64_t id)
  {
    knownPeers.push_back(KnownPeer{ { ipAddress[0], ipAddress[1] }, port, id });
  }

  void addKnownPeerFromNetworkedNode(NetworkedNode* nn);

  static bool peerLocallyReachable(NetworkedNode* from, const KnownPeer& b);

  private:
  friend class cereal::access;

  int64_t id;
  uint32_t availableWorkers;
  uint64_t workQueueCapacity;
  uint64_t workQueueSize;
  uint64_t maximumCPUFrequency;
  uint32_t uptime;
  uint16_t udpListenPort;
  uint16_t tcpListenPort;
  bool daemonMode;
  std::string name;

  KnownPeersVector knownPeers;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(name),
       CEREAL_NVP(id),
       CEREAL_NVP(availableWorkers),
       CEREAL_NVP(workQueueCapacity),
       CEREAL_NVP(workQueueSize),
       CEREAL_NVP(maximumCPUFrequency),
       CEREAL_NVP(uptime),
       CEREAL_NVP(udpListenPort),
       CEREAL_NVP(tcpListenPort),
       CEREAL_NVP(daemonMode),
       CEREAL_NVP(knownPeers));
  }
};
}
}

#endif
