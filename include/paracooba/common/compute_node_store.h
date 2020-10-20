#ifndef PARACOOBA_COMPUTE_NODE_STORE_H
#define PARACOOBA_COMPUTE_NODE_STORE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct parac_compute_node_store;
struct parac_compute_node;

typedef struct parac_compute_node* (*parac_compute_node_store_get_or_create)(
  struct parac_compute_node_store*,
  parac_id id);

typedef bool (*parac_compute_node_store_has)(struct parac_compute_node_store*,
                                             parac_id id);

typedef struct parac_compute_node_store {
  parac_compute_node_store_get_or_create get;
  parac_compute_node_store_has has;
  struct parac_compute_node* this_node;

  void* userdata;
} parac_compute_node_store;

#ifdef __cplusplus
}
#endif

#endif
