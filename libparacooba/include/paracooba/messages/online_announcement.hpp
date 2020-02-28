#ifndef PARACOOBA_MESSAGES_ONLINE_ANNOUNCEMENT
#define PARACOOBA_MESSAGES_ONLINE_ANNOUNCEMENT

#include <cereal/access.hpp>
#include <cstdint>
#include <optional>

#include "node.hpp"

namespace paracooba {
namespace messages {
class OnlineAnnouncement
{
  public:
  OnlineAnnouncement() {}
  OnlineAnnouncement(const Node& node)
    : node(node)
  {}
  OnlineAnnouncement(Node&& node)
    : node(std::move(node))
  {}
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
