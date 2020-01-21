#ifndef PARACUBER_MESSAGES_NODE
#define PARACUBER_MESSAGES_NODE

#include <cstdint>
#include <string>

#include <cereal/access.hpp>
#include <cereal/types/string.hpp>

namespace paracuber {
namespace messages {
class Node
{
  public:
  Node() {}
  virtual ~Node() {}

  const std::string& getName() const { return name; };
  int64_t getId() const { return id; }
  uint16_t getAvailableWorkers() const { return availableWorkers; }
  uint64_t getWorkQueueCapacity() const { return workQueueCapacity; }
  uint64_t getWorkQueueSize() const { return workQueueSize; }
  uint64_t getMaximumCPUFrequency() const { return maximumCPUFrequency; }
  uint32_t getUptime() const { return uptime; }
  uint16_t getUdpListenPort() const { return udpListenPort; }
  uint16_t getTcpListenPort() const { return tcpListenPort; }
  bool getDaemonMode() const { return daemonMode; }

  private:
  friend class cereal::access;

  int64_t id;
  uint16_t availableWorkers;
  uint64_t workQueueCapacity;
  uint64_t workQueueSize;
  uint64_t maximumCPUFrequency;
  uint32_t uptime;
  uint16_t udpListenPort;
  uint16_t tcpListenPort;
  bool daemonMode;
  std::string name;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(id),
       CEREAL_NVP(availableWorkers),
       CEREAL_NVP(workQueueCapacity),
       CEREAL_NVP(workQueueSize),
       CEREAL_NVP(maximumCPUFrequency),
       CEREAL_NVP(uptime),
       CEREAL_NVP(udpListenPort),
       CEREAL_NVP(tcpListenPort),
       CEREAL_NVP(daemonMode));
  }
};
}
}

#endif
