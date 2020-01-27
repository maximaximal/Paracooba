#ifndef PARACUBER_MESSAGES_DAEMON_CONTEXT
#define PARACUBER_MESSAGES_DAEMON_CONTEXT

#include <cstdint>
#include <string>
#include <vector>

#include <cereal/access.hpp>

namespace paracuber {
namespace messages {
class DaemonContext
{
  public:
  DaemonContext() {}
  DaemonContext(int64_t originator, uint8_t state, uint64_t factorySize)
    : originator(originator)
    , state(state)
    , factorySize(factorySize)
  {}
  ~DaemonContext() {}

  int64_t getOriginator() const { return originator; }
  uint8_t getState() const { return state; }
  uint64_t getFactorySize() const { return factorySize; }

  private:
  friend class cereal::access;

  int64_t originator;
  uint8_t state;
  uint64_t factorySize;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(originator), CEREAL_NVP(state), CEREAL_NVP(factorySize));
  }
};
}
}

#endif
