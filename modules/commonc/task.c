#include <assert.h>

#include "paracooba/common/status.h"
#include <paracooba/common/task.h>

#include <parac_common_export.h>

PARAC_COMMON_EXPORT parac_task_state
parac_task_default_assess(parac_task* t) {
  assert(t);

  if(t->state & PARAC_TASK_SPLITTED) {
    if(t->left_result != PARAC_PENDING && t->right_result != PARAC_PENDING) {
      t->state |= PARAC_TASK_SPLITS_DONE;
    }

    if(t->state & PARAC_TASK_SPLITS_DONE) {
      t->state &= ~PARAC_TASK_WAITING_FOR_SPLITS;

      // SAT & UNSAT propagation.
      if(t->left_result == PARAC_SAT || t->right_result == PARAC_SAT) {
        t->result = PARAC_SAT;
      } else if(t->left_result == PARAC_UNSAT &&
                t->right_result == PARAC_UNSAT) {
        t->result = PARAC_UNSAT;
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
