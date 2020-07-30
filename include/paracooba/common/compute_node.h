#ifndef PARACOOBA_COMMON_COMPUTE_NODE_H
#define PARACOOBA_COMMON_COMPUTE_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "two_way_messaging_queue.h"
#include "types.h"

typedef struct parac_compute_node {
  parac_two_way_messaging_queue messaging_queue;

  parac_id id;

  void* userdata;
} parac_compute_node;

#ifdef __cplusplus
}
#endif

#endif
