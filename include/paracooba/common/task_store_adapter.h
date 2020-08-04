#ifndef PARACOOBA_COMMON_TASK_STORE_ADAPTER_H
#define PARACOOBA_COMMON_TASK_STORE_ADAPTER_H

#include <paracooba/common/task_store.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_task_store;

typedef struct parac_task_store_adapter {
  struct parac_task_store store;

  struct parac_task_store* _target;
} parac_task_store_adapter;

void
parac_task_store_adapter_init(parac_task_store_adapter* adapter);

#ifdef __cplusplus
}
#endif

#endif
