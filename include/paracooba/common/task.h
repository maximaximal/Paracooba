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

  PARAC_TASK_ERROR = 1 << 6,

  PARAC_TASK_WAITING_FOR_RIGHT =
    PARAC_TASK_LEFT_DONE | PARAC_TASK_WAITING_FOR_SPLITS | PARAC_TASK_SPLITTED,
  PARAC_TASK_WAITING_FOR_LEFT =
    PARAC_TASK_RIGHT_DONE | PARAC_TASK_WAITING_FOR_SPLITS | PARAC_TASK_SPLITTED,
  PARAC_TASK_SPLITS_DONE =
    PARAC_TASK_LEFT_DONE | PARAC_TASK_RIGHT_DONE | PARAC_TASK_SPLITTED,
  PARAC_TASK_ALL_DONE = PARAC_TASK_LEFT_DONE | PARAC_TASK_RIGHT_DONE |
                        PARAC_TASK_DONE | PARAC_TASK_SPLITTED,
} parac_task_state;

struct parac_task;
struct parac_message;

typedef parac_task_state (*parac_task_assess_func)(struct parac_task*);
typedef parac_status (*parac_task_work_func)(struct parac_task*);
typedef parac_status (*parac_task_serialize_func)(struct parac_task*,
                                                  struct parac_message*);
typedef parac_status (*parac_task_free_userdata_func)(struct parac_task*);

typedef struct parac_task {
  parac_task_state state;
  parac_status result;
  parac_status left_result;
  parac_status right_result;
  bool stop;
  parac_path path;
  parac_id received_from;
  parac_id offloaded_to;

  void* userdata;
  parac_task_assess_func assess;
  parac_task_work_func work;
  parac_task_serialize_func serialize;
  parac_task_free_userdata_func free_userdata;
} parac_task;

#ifdef __cplusplus
}

class parac_task_wrapper : public parac_task {
  public:
  parac_task_wrapper() {
    state = PARAC_TASK_NEW;
    result = PARAC_PENDING;
    left_result = PARAC_PENDING;
    right_result = PARAC_PENDING;
    path.rep = PARAC_PATH_EXPLICITLY_UNKNOWN;
    userdata = nullptr;
    work = nullptr;
    assess = nullptr;
    received_from = 0;
    offloaded_to = 0;
  }
  ~parac_task_wrapper() { free_userdata(this); }

  bool stateActive(parac_task_state s) const { return state & s; }

  parac_status doWork() {
    if(!work)
      return PARAC_UNDEFINED;
    return work(this);
  }
  parac_task_state doAssess() {
    if(!assess)
      return PARAC_TASK_ERROR;
    return assess(this);
  }
  parac_status doSerialize(parac_message& msg) {
    if(!serialize)
      return PARAC_UNDEFINED;
    return serialize(this, &msg);
  }
};
#endif

#endif
