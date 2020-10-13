#include <paracooba/common/message_kind.h>

#include <parac_common_export.h>

PARAC_COMMON_EXPORT const char*
parac_message_kind_to_str(parac_message_kind kind) {
  switch(kind) {
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
    case PARAC_MESSAGE_CNF_TREE_NODE_STATUS_REQUEST:
      return "CNFTreeNodeStatusRequest";
    case PARAC_MESSAGE_CNF_TREE_NODE_STATUS_REPLY:
      return "CNFTreeNodeStatusReply";
    case PARAC_MESSAGE_NEW_REMOTE_CONNECTED:
      return "NewRemoteConnected";
    case PARAC_MESSAGE_JOB_PATH:
      return "JobPath";
    case PARAC_MESSAGE_JOB_RESULT:
      return "JobResult";
    case PARAC_MESSAGE_JOB_INITIATOR:
      return "JobInitiator";
    case PARAC_MESSAGE_FILE:
      return "File";
    case PARAC_MESSAGE_ACK:
      return "Ack";
    case PARAC_MESSAGE_END:
      return "End";
  }
  return "";
}
