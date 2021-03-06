#ifndef PARACOOBA_COMMON_MESSAGE_KIND_H
#define PARACOOBA_COMMON_MESSAGE_KIND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef enum parac_message_kind {
  PARAC_MESSAGE_UNKNOWN,
  PARAC_MESSAGE_ONLINE_ANNOUNCEMENT,
  PARAC_MESSAGE_OFFLINE_ANNOUNCEMENT,
  PARAC_MESSAGE_ANNOUNCEMENT_REQUEST,
  PARAC_MESSAGE_NODE_STATUS,
  PARAC_MESSAGE_NODE_DESCRIPTION,
  PARAC_MESSAGE_NEW_REMOTES,
  PARAC_MESSAGE_TASK_REPARENT,
  PARAC_MESSAGE_TASK_RESULT,

  PARAC_MESSAGE_SOLVER_DESCRIPTION,
  PARAC_MESSAGE_SOLVER_TASK,

  PARAC_MESSAGE_FILE,
  PARAC_MESSAGE_ACK,
  PARAC_MESSAGE_END,
  PARAC_MESSAGE_KEEPALIVE,

  PARAC_MESSAGE__COUNT
} parac_message_kind;

const char*
parac_message_kind_to_str(parac_message_kind kind);

bool
parac_message_kind_is_for_solver(parac_message_kind kind);

#ifdef __cplusplus
}

#include <iostream>

inline std::ostream&
operator<<(std::ostream& o, parac_message_kind kind) {
  return o << parac_message_kind_to_str(kind);
}

#endif

#endif
