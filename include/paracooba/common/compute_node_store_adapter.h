#ifndef PARACOOBA_COMMON_COMPUTE_NODE_STORE_ADAPTER_H
#define PARACOOBA_COMMON_COMPUTE_NODE_STORE_ADAPTER_H

#include <paracooba/common/compute_node_store.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_compute_node_store;

typedef struct parac_compute_node_store_adapter {
  struct parac_compute_node_store store;

  struct parac_compute_node_store* _target;
} parac_compute_node_store_adapter;

void
parac_compute_node_store_adapter_init(
  parac_compute_node_store_adapter* adapter);

#ifdef __cplusplus
}
#endif

#endif
