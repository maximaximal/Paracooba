#ifndef PARACUBER_MESSAGES_ONLINE_ANNOUNCEMENT
#define PARACUBER_MESSAGES_ONLINE_ANNOUNCEMENT

#include <cereal/access.hpp>
#include <cstdint>
#include <optional>

#include "node.hpp"

namespace paracuber {
namespace messages {
class OnlineAnnouncement
{
  public:
  explicit OnlineAnnouncement() {}
  virtual ~OnlineAnnouncement() {}

  const Node& getNode() const { return node; }

  private:
  friend class cereal::access;

  Node node;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(node);
  }
};
}
}

#endif
