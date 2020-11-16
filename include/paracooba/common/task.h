#ifndef PARACOOBA_COMMON_TASK_H
#define PARACOOBA_COMMON_TASK_H

#include "path.h"
#include "status.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum parac_task_state {
  PARAC_TASK_NEW = 0,
  PARAC_TASK_SPLITTED = 1 << 0,
  PARAC_TASK_WORK_AVAILABLE = 1 << 1,
  PARAC_TASK_WAITING_FOR_SPLITS = 1 << 2,
  PARAC_TASK_LEFT_DONE = 1 << 3,
  PARAC_TASK_RIGHT_DONE = 1 << 4,
  PARAC_TASK_DONE = 1 << 5,
  PARAC_TASK_OFFLOADED = 1 << 6,

  PARAC_TASK_ERROR = 1 << 7,

  PARAC_TASK_WAITING_FOR_RIGHT =
    PARAC_TASK_LEFT_DONE | PARAC_TASK_WAITING_FOR_SPLITS | PARAC_TASK_SPLITTED,
  PARAC_TASK_WAITING_FOR_LEFT =
    PARAC_TASK_RIGHT_DONE | PARAC_TASK_WAITING_FOR_SPLITS | PARAC_TASK_SPLITTED,
  PARAC_TASK_SPLITS_DONE =
    PARAC_TASK_LEFT_DONE | PARAC_TASK_RIGHT_DONE | PARAC_TASK_SPLITTED,
  PARAC_TASK_ALL_DONE = PARAC_TASK_LEFT_DONE | PARAC_TASK_RIGHT_DONE |
                        PARAC_TASK_DONE | PARAC_TASK_SPLITTED,
} parac_task_state;

/** @brief Utility function to check if a task is done, irregarding splitted or
 * not. */
bool
parac_task_state_is_done(parac_task_state state);

struct parac_task;
struct parac_message;

typedef parac_task_state (*parac_task_assess_func)(struct parac_task*);
typedef parac_status (*parac_task_work_func)(struct parac_task*);
typedef parac_status (*parac_task_serialize_func)(struct parac_task*,
                                                  struct parac_message*);
typedef parac_status (*parac_task_free_userdata_func)(struct parac_task*);

typedef struct parac_task {
  parac_task_state last_state;
  parac_task_state state;
  parac_status result;
  parac_status left_result;
  parac_status right_result;
  bool stop;
  parac_path path;
  parac_id received_from;
  parac_id offloaded_to;
  parac_id originator;

  void* userdata;
  parac_task_assess_func assess;
  parac_task_work_func work;
  parac_task_serialize_func serialize;
  parac_task_free_userdata_func free_userdata;

  struct parac_task* parent_task;
} parac_task;

void
parac_task_init(parac_task* t);

/** @brief Upholds the left and right split invariant and propagates SAT & UNSAT
 */
parac_task_state
parac_task_default_assess(struct parac_task* task);

#ifdef __cplusplus
}
inline parac_task_state
operator|(parac_task_state a, parac_task_state b) {
  return static_cast<parac_task_state>(static_cast<int>(a) |
                                       static_cast<int>(b));
}
inline parac_task_state operator&(parac_task_state a, parac_task_state b) {
  return static_cast<parac_task_state>(static_cast<int>(a) &
                                       static_cast<int>(b));
}

inline std::ostream&
operator<<(std::ostream& o, parac_task_state s) {
  o << "( ";
  if(s == PARAC_TASK_NEW)
    o << "new ";
  if(s & PARAC_TASK_SPLITTED)
    o << "splitted ";
  if(s & PARAC_TASK_WORK_AVAILABLE)
    o << "work-available ";
  if(s & PARAC_TASK_WAITING_FOR_SPLITS)
    o << "waiting-for-splits ";
  if(s & PARAC_TASK_LEFT_DONE)
    o << "left-done ";
  if(s & PARAC_TASK_RIGHT_DONE)
    o << "right-done ";
  if(s & PARAC_TASK_DONE)
    o << "done ";
  if(s & PARAC_TASK_OFFLOADED)
    o << "offloaded ";
  if(s & PARAC_TASK_ERROR)
    o << "error ";
  o << ")";
  return o;
}
#endif

#endif
