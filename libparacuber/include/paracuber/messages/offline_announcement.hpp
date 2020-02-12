#ifndef PARACUBER_MESSAGES_OFFLINE_ANNOUNCEMENT
#define PARACUBER_MESSAGES_OFFLINE_ANNOUNCEMENT

#include <cstdint>
#include <optional>

#include <cereal/access.hpp>
#include <cereal/types/string.hpp>

namespace paracuber {
namespace messages {
class OfflineAnnouncement
{
  public:
  OfflineAnnouncement() {}
  OfflineAnnouncement(const std::string& reason)
    : reason(reason)
  {}
  virtual ~OfflineAnnouncement() {}

  const std::string& getReason() const { return reason; }

  private:
  friend class cereal::access;

  std::string reason;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(reason));
  }
};
}
}

#endif
