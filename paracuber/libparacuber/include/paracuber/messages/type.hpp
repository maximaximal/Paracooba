#ifndef PARACUBER_MESSAGES_TYPE
#define PARACUBER_MESSAGES_TYPE

namespace paracuber {
namespace messages {
enum class Type
{
  AnnouncementRequest,
  OnlineAnnouncement,
  OfflineAnnouncement,
  NodeStatus,
  CNFTreeNodeStatusRequest,
  CNFTreeNodeStatusReply,
  Unknown
};

constexpr const char*
TypeToStr(Type type)
{
  switch(type) {
    case Type::AnnouncementRequest:
      return "AnnouncementRequest";
    case Type::OnlineAnnouncement:
      return "OnlineAnnouncement";
    case Type::OfflineAnnouncement:
      return "OfflineAnnouncement";
    case Type::NodeStatus:
      return "NodeStatus";
    case Type::CNFTreeNodeStatusRequest:
      return "CNFTreeNodeStatusRequest";
    case Type::CNFTreeNodeStatusReply:
      return "CNFTreeNodeStatusReply";
  }
  return "Unknown Type";
}
}
}

#endif
