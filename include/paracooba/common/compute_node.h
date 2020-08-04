#ifndef PARACOOBA_COMMON_COMPUTE_NODE_H
#define PARACOOBA_COMMON_COMPUTE_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

struct parac_compute_node;
struct parac_message;

typedef void (*parac_compute_node_message_func)(
  struct parac_compute_node* compute_node,
  struct parac_message* msg);

typedef struct parac_compute_node {
  parac_compute_node_message_func send_message_to;     /// Set by Communicator.
  parac_compute_node_message_func receive_message_from;/// Set by Broker.

  parac_id id;

  void* broker_userdata;      /// Set by Broker.
  void* communicator_userdata;/// Set by Communicator.
} parac_compute_node;

#ifdef __cplusplus
}
#endif

#endif
