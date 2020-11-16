#include <assert.h>

#include "paracooba/common/status.h"
#include <paracooba/common/task.h>

#include <parac_common_export.h>

PARAC_COMMON_EXPORT parac_task_state
parac_task_default_assess(parac_task* t) {
  assert(t);

  if(t->state & PARAC_TASK_SPLITTED) {
    if(t->left_result != PARAC_PENDING) {
      t->state |= PARAC_TASK_LEFT_DONE;
    }
    if(t->right_result != PARAC_PENDING) {
      t->state |= PARAC_TASK_RIGHT_DONE;
    }

    if((t->state & PARAC_TASK_LEFT_DONE) &&
       (t->state & PARAC_TASK_RIGHT_DONE)) {
      t->state &= ~PARAC_TASK_WAITING_FOR_SPLITS;
      t->state |= PARAC_TASK_SPLITS_DONE;
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
  t->received_from = 0;
  t->offloaded_to = 0;
  t->originator = 0;
  t->parent_task = NULL;
}
