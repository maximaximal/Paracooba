#include <paracooba/common/message_kind.h>

#include <parac_common_export.h>

PARAC_COMMON_EXPORT const char*
parac_message_kind_to_str(parac_message_kind kind) {
  switch(kind) {
    case PARAC_MESSAGE__COUNT:
    case PARAC_MESSAGE_UNKNOWN:
      return "Unknown";
    case PARAC_MESSAGE_ONLINE_ANNOUNCEMENT:
      return "OnlineAnnouncement";
    case PARAC_MESSAGE_OFFLINE_ANNOUNCEMENT:
      return "OfflineAnnouncement";
    case PARAC_MESSAGE_ANNOUNCEMENT_REQUEST:
      return "AnnouncementRequest";
    case PARAC_MESSAGE_NODE_STATUS:
      return "NodeStatus";
    case PARAC_MESSAGE_NODE_DESCRIPTION:
      return "NodeDescription";
    case PARAC_MESSAGE_TASK_REPARENT:
      return "ReparentTask";
    case PARAC_MESSAGE_NEW_REMOTES:
      return "NewRemotes";
    case PARAC_MESSAGE_SOLVER_DESCRIPTION:
      return "Solver-Description";
    case PARAC_MESSAGE_SOLVER_TASK:
      return "Solver-Task";
    case PARAC_MESSAGE_TASK_RESULT:
      return "Task-Result";
    case PARAC_MESSAGE_FILE:
      return "File";
    case PARAC_MESSAGE_ACK:
      return "Ack";
    case PARAC_MESSAGE_END:
      return "End";
    case PARAC_MESSAGE_KEEPALIVE:
      return "Keep-Alive";
  }
  return "";
}

PARAC_COMMON_EXPORT bool
parac_message_kind_is_for_solver(parac_message_kind kind) {
  switch(kind) {
    case PARAC_MESSAGE_SOLVER_TASK:
    case PARAC_MESSAGE_SOLVER_DESCRIPTION:
      return true;
    default:
      return false;
  }
}
