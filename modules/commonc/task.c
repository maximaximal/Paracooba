#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "paracooba/common/message.h"
#include "paracooba/common/status.h"
#include <paracooba/common/path.h>
#include <paracooba/common/task.h>
#include <paracooba/module.h>

#include <paracooba/common/log.h>

#include <parac_common_export.h>

typedef struct parac_task_result_packet {
  void* task_ptr;
  uint32_t result;
} parac_task_result_packet;

static_assert(sizeof(char) * PARAC_MESSAGE_INLINE_DATA_SIZE >=
                sizeof(parac_task_result_packet),
              "Inline data must be able to fit parac_task_result_packet!");

static void
notify_result_cb(parac_message* msg, parac_status status) {
  assert(msg);
  assert(msg->userdata);
  parac_handle* handle = msg->userdata;
  assert(handle);

  if(status == PARAC_PATH_NOT_FOUND_ERROR ||
     status == PARAC_COMPUTE_NODE_NOT_FOUND_ERROR) {
    parac_log(PARAC_GENERAL,
              PARAC_FATAL,
              "Received a Path Not Found or Node Not Found error in notify "
              "result cb! Killing local node is the only resort.");

    handle->exit_status = PARAC_GENERIC_ERROR;
    handle->request_exit(handle);
  }
}

static void
notify_result(parac_task* t) {
  if(!t || !t->received_from || !t->received_from->send_message_to) {
    parac_log(PARAC_GENERAL,
              PARAC_LOCALERROR,
              "Following assert in notift_result comes from this thread.");
  }
  assert(t);
  assert(t->received_from);
  assert(t->received_from->send_message_to);
  assert(t->parent_task_);
  assert(t->handle);

  parac_message msg;
  msg.kind = PARAC_MESSAGE_TASK_RESULT;

  memset(msg.inline_data, 0, PARAC_MESSAGE_INLINE_DATA_SIZE);

  parac_task_result_packet* res = (void*)msg.inline_data;
  res->task_ptr = t->parent_task_;
  res->result = t->result;

  msg.data_to_be_freed = false;
  msg.data_is_inline = true;
  msg.userdata = t->handle;
  msg.cb = notify_result_cb;
  msg.originator_id = t->originator;
  msg.length = sizeof(parac_task_result_packet);
  t->received_from->send_message_to(t->received_from, &msg);
}

PARAC_COMMON_EXPORT parac_status
parac_task_result_packet_get_result(void* result) {
  parac_task_result_packet* res = result;
  return res->result;
}
PARAC_COMMON_EXPORT struct parac_task*
parac_task_result_packet_get_task_ptr(void* result) {
  parac_task_result_packet* res = result;
  return res->task_ptr;
}

static void
early_abort_task(parac_task* t) {
  assert(t);
  if(t->terminate)
    t->terminate(t);
}

static bool
existential_assess(parac_task* t, bool terminate_children) {
  if(t->left_result == PARAC_SAT || t->right_result == PARAC_SAT) {
    t->result = PARAC_SAT;

    if(terminate_children) {
      if(t->left_result == PARAC_SAT && t->right_child_ &&
         !parac_task_state_is_done(t->right_child_->state)) {
        early_abort_task(t->right_child_);
      } else if(t->right_result == PARAC_SAT && t->left_child_ &&
                !parac_task_state_is_done(t->left_child_->state)) {
        early_abort_task(t->left_child_);
      }
    }

    return true;
  } else if(t->left_result == PARAC_UNSAT && t->right_result == PARAC_UNSAT) {
    t->result = PARAC_UNSAT;
    return true;
  }
  return false;
}

static bool
universal_assess(parac_task* t, bool terminate_children) {
  if(t->left_result == PARAC_UNSAT || t->right_result == PARAC_UNSAT) {
    t->result = PARAC_UNSAT;

    if(terminate_children) {
      if(t->left_result == PARAC_UNSAT && t->right_child_ &&
         !parac_task_state_is_done(t->right_child_->state)) {
        early_abort_task(t->right_child_);
      } else if(t->right_result == PARAC_UNSAT && t->left_child_ &&
                !parac_task_state_is_done(t->left_child_->state)) {
        early_abort_task(t->left_child_);
      }
    }

    return true;
  } else if(t->left_result == PARAC_SAT && t->right_result == PARAC_SAT) {
    t->result = PARAC_SAT;
    return true;
  }
  return false;
}

typedef bool (*assess_func)(parac_task*, bool);

static parac_task_state
shared_assess(parac_task* t, assess_func a, bool terminate_children) {
  assert(t);
  assert(t->task_store);

  if(t->state & PARAC_TASK_SPLITTED) {
    if(t->left_result != PARAC_PENDING && t->right_result != PARAC_PENDING) {
      t->state |= PARAC_TASK_SPLITS_DONE;
    }

    if(t->state & PARAC_TASK_SPLITS_DONE) {
      t->state &= ~PARAC_TASK_WAITING_FOR_SPLITS;

      bool notify = a(t, terminate_children);

      if(notify && t->received_from) {
        notify_result(t);
      }
    }
  } else {
    if(t->received_from && (t->result != PARAC_PENDING)) {
      notify_result(t);
    }
  }

  return t->state;
}

PARAC_COMMON_EXPORT parac_task_state
parac_task_default_assess(parac_task* t) {
  return shared_assess(t, existential_assess, false);
}

PARAC_COMMON_EXPORT parac_task_state
parac_task_qbf_existential_assess(parac_task* t) {
  return shared_assess(t, existential_assess, true);
}

PARAC_COMMON_EXPORT parac_task_state
parac_task_qbf_universal_assess(parac_task* t) {
  return shared_assess(t, universal_assess, true);
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
  t->handle = NULL;
  t->serialize = NULL;
  t->stop = false;
  t->terminate = NULL;
  t->worker = 0;
  t->pre_path_sorting_critereon = 0;
  t->post_path_sorting_critereon = 0;
}

PARAC_COMMON_EXPORT
bool
parac_task_state_is_done(parac_task_state s) {
  return !(s & PARAC_TASK_WORK_AVAILABLE) && !(s & PARAC_TASK_WORKING) &&
         ((!(s & PARAC_TASK_SPLITTED) && s & PARAC_TASK_DONE) ||
          ((s & PARAC_TASK_SPLITTED) && (s & PARAC_TASK_SPLITS_DONE)));
}

PARAC_COMMON_EXPORT bool
parac_task_compare(const parac_task* l, const parac_task* r) {
  assert(l);
  assert(r);
  bool res = false;
  if(l->pre_path_sorting_critereon != r->pre_path_sorting_critereon) {
    return l->pre_path_sorting_critereon < r->pre_path_sorting_critereon;
  }
  size_t l_path_length = parac_path_length(l->path);
  size_t r_path_length = parac_path_length(r->path);
  if(l_path_length != r_path_length) {
    return l_path_length > r_path_length;
  }
  return l->post_path_sorting_critereon < r->post_path_sorting_critereon;
}
