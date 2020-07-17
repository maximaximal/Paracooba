#ifndef PARACOOBA_TRACER_CONVERSIONS_HPP
#define PARACOOBA_TRACER_CONVERSIONS_HPP

#include "messages/jobdescription.hpp"
#include "messages/type.hpp"
#include "tracer.hpp"

namespace paracooba {
constexpr traceentry::MessageKind
JobDescriptionKindToTraceMessageKind(messages::JobDescription::Kind kind)
{
  switch(kind) {
    case messages::JobDescription::Kind::Path:
      return traceentry::MessageKind::JobPath;
    case messages::JobDescription::Kind::Result:
      return traceentry::MessageKind::JobResult;
    case messages::JobDescription::Kind::Initiator:
      return traceentry::MessageKind::JobInitiator;
    default:
      return traceentry::MessageKind::Unknown;
  }
}

constexpr traceentry::MessageKind
MessageTypeToTraceMessageKind(messages::Type type)
{
  switch(type) {
    case messages::Type::AnnouncementRequest:
      return traceentry::MessageKind::AnnouncementRequest;
    case messages::Type::OnlineAnnouncement:
      return traceentry::MessageKind::OnlineAnnouncement;
    case messages::Type::OfflineAnnouncement:
      return traceentry::MessageKind::OfflineAnnouncement;
    case messages::Type::NodeStatus:
      return traceentry::MessageKind::NodeStatus;
    case messages::Type::CNFTreeNodeStatusRequest:
      return traceentry::MessageKind::CNFTreeNodeStatusRequest;
    case messages::Type::NewRemoteConnected:
      return traceentry::MessageKind::NewRemoteConnected;
    default:
      return traceentry::MessageKind::Unknown;
  }
}
}

#endif
