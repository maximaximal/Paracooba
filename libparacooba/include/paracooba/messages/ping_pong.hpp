#ifndef PARACOOBA_MESSAGES_PING_PONG
#define PARACOOBA_MESSAGES_PING_PONG

#include <cereal/access.hpp>

#include "../types.hpp"

namespace paracooba {
namespace messages {

/** @brief Will be answered by a Pong message. */
class Ping
{
  public:
  Ping() {}
  Ping(ID referenceNumber)
    : referenceNumber(referenceNumber)
  {}
  virtual ~Ping() {}

  ID getReferenceNumber() const { return referenceNumber; }

  private:
  friend class cereal::access;

  ID referenceNumber;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(referenceNumber);
  }
};

/** @brief Answer to a Ping message. */
class Pong
{
  public:
  Pong() {}
  Pong(ID referenceNumber)
    : referenceNumber(referenceNumber)
  {}
  Pong(const Ping& ping)
    : referenceNumber(ping.getReferenceNumber())
  {}
  virtual ~Pong() {}

  ID getReferenceNumber() const { return referenceNumber; }

  private:
  friend class cereal::access;

  ID referenceNumber;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(referenceNumber);
  }
};
}
}

#endif
