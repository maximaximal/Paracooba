#ifndef PARACUBER_MESSAGES_ANNOUNCEMENT_REQUEST
#define PARACUBER_MESSAGES_ANNOUNCEMENT_REQUEST

#include <cstdint>
#include <string>
#include <variant>

#include <cereal/access.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>

#include "node.hpp"

namespace paracuber {
namespace messages {
class AnnouncementRequest
{
  public:
  AnnouncementRequest()
    : nameMatch(false)
  {}
  AnnouncementRequest(const Node& requester)
    : requester(requester)
    , nameMatch(false)
  {}
  AnnouncementRequest(const Node& requester, const std::string& regex)
    : requester(requester)
    , nameMatch(regex)
  {}
  AnnouncementRequest(const Node& requester, int64_t id)
    : requester(requester)
    , nameMatch(id)
  {}
  virtual ~AnnouncementRequest() {}

  enum NameMatch
  {
    NO_RESTRICTION,
    REGEX,
    ID
  };

  const Node& getRequester() const { return requester; }
  const std::string getRegexMatch() const
  {
    return std::get<std::string>(nameMatch);
  }
  int64_t getIdMatch() const { return std::get<int64_t>(nameMatch); }

  NameMatch getNameMatchType() const
  {
    if(std::holds_alternative<bool>(nameMatch))
      return NO_RESTRICTION;
    if(std::holds_alternative<std::string>(nameMatch))
      return REGEX;
    if(std::holds_alternative<int64_t>(nameMatch))
      return ID;
    return NO_RESTRICTION;
  }

  private:
  friend class cereal::access;

  Node requester;

  using NameMatchVariant = std::variant<bool, std::string, int64_t>;
  NameMatchVariant nameMatch;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(requester), CEREAL_NVP(nameMatch));
  }
};
}
}

#endif
