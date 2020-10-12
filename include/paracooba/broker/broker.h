#ifndef PARACOOBA_MODULE_BROKER_H
#define PARACOOBA_MODULE_BROKER_H

#include <paracooba/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_task_store;
struct parac_compute_node_store;
struct parac_compute_node;
struct parac_module;

typedef struct parac_module_broker {
  parac_task_store* task_store;
  parac_compute_node_store* compute_node_store;
} parac_module_broker;

#ifdef __cplusplus
}
#endif

#endif
