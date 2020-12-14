#ifndef PARACOOBA_COMPUTE_NODE_STORE_H
#define PARACOOBA_COMPUTE_NODE_STORE_H

#include "compute_node.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct parac_compute_node_store;

typedef struct parac_compute_node* (*parac_compute_node_store_get_or_create)(
  struct parac_compute_node_store*,
  parac_id id);

typedef struct parac_compute_node* (
  *parac_compute_node_store_get_or_create_with_connection)(
  struct parac_compute_node_store*,
  parac_id id,
  parac_compute_node_free_func communicator_free,
  void* communicator_userdata,
  parac_compute_node_message_func send_message_func,
  parac_compute_node_file_func send_file_func);

typedef bool (*parac_compute_node_store_has)(struct parac_compute_node_store*,
                                             parac_id id);

typedef struct parac_compute_node_store {
  parac_compute_node_store_get_or_create get;
  parac_compute_node_store_get_or_create_with_connection get_with_connection;
  parac_compute_node_store_has has;
  struct parac_compute_node* this_node;

  void* userdata;
} parac_compute_node_store;

#ifdef __cplusplus
}
#endif

#endif
