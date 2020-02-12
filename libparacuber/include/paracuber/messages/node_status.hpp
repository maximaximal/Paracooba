#ifndef PARACUBER_MESSAGES_NODE_STATUS
#define PARACUBER_MESSAGES_NODE_STATUS

#include <cstdint>
#include <optional>

#include <cereal/access.hpp>
#include <cereal/types/optional.hpp>

#include "daemon.hpp"

namespace paracuber {
namespace messages {
class NodeStatus
{
  public:
  using OptionalDaemon = std::optional<Daemon>;

  NodeStatus() {}
  NodeStatus(uint64_t workQueueSize, OptionalDaemon daemon = std::nullopt)
    : workQueueSize(workQueueSize)
    , daemon(daemon)
  {}
  virtual ~NodeStatus() {}

  uint64_t getWorkQueueSize() const { return workQueueSize; }
  Daemon& getDaemon() { return daemon.value(); }
  const Daemon& getDaemon() const { return daemon.value(); }
  bool isDaemon() const { return daemon.has_value(); }

  private:
  friend class cereal::access;

  uint64_t workQueueSize;
  OptionalDaemon daemon;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(workQueueSize), CEREAL_NVP(daemon));
  }
};
}
}

#endif
