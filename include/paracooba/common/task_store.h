#ifndef PARACOOBA_COMMON_TASK_STORE_H
#define PARACOOBA_COMMON_TASK_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct parac_task_store;
struct parac_task;

typedef bool (*parac_task_store_empty)(struct parac_task_store*);
typedef size_t (*parac_task_store_get_size)(struct parac_task_store*);
typedef void (*parac_task_store_push)(struct parac_task_store*,
                                      const struct parac_task*);

typedef void (*parac_task_store_pop_top)(struct parac_task_store*,
                                         struct parac_task*);
typedef void (*parac_task_store_pop_bottom)(struct parac_task_store*,
                                            struct parac_task*);

typedef struct parac_task_store {
  parac_task_store_empty empty;
  parac_task_store_get_size get_size;
  parac_task_store_push push;
  parac_task_store_pop_top pop_top;
  parac_task_store_pop_bottom pop_bottom;

  void* userdata;
} parac_task_store;

#ifdef __cplusplus
}
#endif

#endif
