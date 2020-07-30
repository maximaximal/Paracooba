#ifndef PARACOOBA_COMMON_TWO_WAY_MESSAGING_QUEUE_H
#define PARACOOBA_COMMON_TWO_WAY_MESSAGING_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "messaging_queue.h"

typedef struct parac_two_way_messaging_queue {
  parac_messaging_queue queue_send;
  parac_messaging_queue queue_receive;
} parac_two_way_messaging_queue;

#ifdef __cplusplus
}
#endif

#endif
