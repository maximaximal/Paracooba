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

typedef bool (*parac_task_store_empty)(struct parac_task_store*);
typedef size_t (*parac_task_store_get_size)(struct parac_task_store*);
typedef struct parac_task* (
  *parac_task_store_new_task)(struct parac_task_store*, parac_path, parac_id);

typedef struct parac_task* (*parac_task_store_pop_top)(
  struct parac_task_store*);
typedef struct parac_task* (*parac_task_store_pop_bottom)(
  struct parac_task_store*);

typedef struct parac_task_store {
  parac_task_store_empty empty;
  parac_task_store_get_size get_size;
  parac_task_store_new_task new_task;
  parac_task_store_pop_top pop_top;
  parac_task_store_pop_bottom pop_bottom;

  void* userdata;
} parac_task_store;

#ifdef __cplusplus
}
#endif

#endif
