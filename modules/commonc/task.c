#include <assert.h>
#include <string.h>

#include "paracooba/common/message.h"
#include "paracooba/common/status.h"
#include <paracooba/common/path.h>
#include <paracooba/common/task.h>

#include <parac_common_export.h>

typedef struct parac_task_result_packet {
  parac_path_type path;
  uint32_t result;
} parac_task_result_packet;

static_assert(sizeof(char) * PARAC_MESSAGE_INLINE_DATA_SIZE >=
                sizeof(parac_task_result_packet),
              "Inline data must be able to fit parac_task_result_packet!");

static void
notify_result(parac_task* t) {
  assert(t);
  assert(t->received_from);
  parac_message msg;
  msg.kind = PARAC_MESSAGE_TASK_RESULT;

  memset(msg.inline_data, 0, PARAC_MESSAGE_INLINE_DATA_SIZE);

  parac_task_result_packet* res = (void*)msg.inline_data;
  res->path = t->path.rep;
  res->result = t->result;

  msg.data_to_be_freed = false;
  msg.data_is_inline = true;
  msg.userdata = NULL;
  msg.cb = NULL;
  msg.originator_id = t->originator;
  msg.length = sizeof(parac_task_result_packet);
  t->received_from->send_message_to(t->received_from, &msg);
}

PARAC_COMMON_EXPORT parac_status
parac_task_result_packet_get_result(void* result) {
  parac_task_result_packet* res = result;
  return res->result;
}
PARAC_COMMON_EXPORT struct parac_path
parac_task_result_packet_get_path(void* result) {
  parac_task_result_packet* res = result;
  struct parac_path p = { .rep = res->path };
  return p;
}

PARAC_COMMON_EXPORT parac_task_state
parac_task_default_assess(parac_task* t) {
  assert(t);

  if(t->state & PARAC_TASK_SPLITTED) {
    if(t->left_result != PARAC_PENDING && t->right_result != PARAC_PENDING) {
      t->state |= PARAC_TASK_SPLITS_DONE;
    }

    if(t->state & PARAC_TASK_SPLITS_DONE) {
      t->state &= ~PARAC_TASK_WAITING_FOR_SPLITS;

      bool notify = false;

      // SAT & UNSAT propagation.
      if(t->left_result == PARAC_SAT || t->right_result == PARAC_SAT) {
        t->result = PARAC_SAT;
        notify = true;
      } else if(t->left_result == PARAC_UNSAT &&
                t->right_result == PARAC_UNSAT) {
        t->result = PARAC_UNSAT;
        notify = true;
      }

      if(notify && t->received_from) {
        notify_result(t);
      }
    }
  }

  return t->state;
}

PARAC_COMMON_EXPORT void
parac_task_init(parac_task* t) {
  t->last_state = PARAC_TASK_NEW;
  t->state = PARAC_TASK_NEW;
  t->result = PARAC_PENDING;
  t->left_result = PARAC_PENDING;
  t->right_result = PARAC_PENDING;
  t->path.rep = PARAC_PATH_EXPLICITLY_UNKNOWN;
  t->userdata = NULL;
  t->work = NULL;
  t->assess = &parac_task_default_assess;
  t->free_userdata = NULL;
  t->received_from = NULL;
  t->offloaded_to = NULL;
  t->originator = 0;
  t->parent_task_ = NULL;
  t->left_child_ = NULL;
  t->right_child_ = NULL;
  t->task_store = NULL;
  t->stop = false;
}

PARAC_COMMON_EXPORT
bool
parac_task_state_is_done(parac_task_state s) {
  return !(s & PARAC_TASK_WORK_AVAILABLE) && !(s & PARAC_TASK_WORKING) &&
         ((!(s & PARAC_TASK_SPLITTED) && s & PARAC_TASK_DONE) ||
          ((s & PARAC_TASK_SPLITTED) && (s & PARAC_TASK_SPLITS_DONE)));
}
