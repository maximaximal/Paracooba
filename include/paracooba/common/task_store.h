#ifndef PARACOOBA_COMMON_TASK_STORE_H
#define PARACOOBA_COMMON_TASK_STORE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct parac_task_store;
struct parac_task;
struct parac_path;
struct parac_compute_node;

typedef bool (*parac_task_store_empty)(struct parac_task_store*);
typedef size_t (*parac_task_store_get_size)(struct parac_task_store*);
typedef struct parac_task* (*parac_task_store_new_task)(
  struct parac_task_store*,
  struct parac_task* creator_task,
  struct parac_path new_path,
  parac_id originator);
typedef void (*parac_task_store_assess_task)(struct parac_task_store*,
                                             struct parac_task*);
typedef void (*parac_task_store_undo_offload)(struct parac_task_store*,
                                              struct parac_task*);
typedef struct parac_task* (*parac_task_store_pop_offload)(
  struct parac_task_store*,
  struct parac_compute_node*);
typedef struct parac_task* (*parac_task_store_pop_work)(
  struct parac_task_store*,
  volatile bool* delete_notification);
typedef void (*parac_task_store_ping_on_work)(struct parac_task_store*);

typedef struct parac_task_store {
  parac_task_store_empty empty;
  parac_task_store_get_size get_size;
  parac_task_store_get_size get_waiting_for_children_size;
  parac_task_store_get_size get_waiting_for_worker_size;
  parac_task_store_get_size get_tasks_being_worked_on_size;
  parac_task_store_new_task new_task;
  parac_task_store_pop_offload pop_offload;
  parac_task_store_pop_work pop_work;
  parac_task_store_assess_task assess_task;
  parac_task_store_undo_offload undo_offload;

  void* userdata;

  void* ping_on_work_userdata;
  parac_task_store_ping_on_work ping_on_work;
} parac_task_store;

#ifdef __cplusplus
}
#endif

#endif
