#ifndef PARACOOBA_COMMON__MESSAGING_QUEUE_H
#define PARACOOBA_COMMON__MESSAGING_QUEUE_H

#include "message.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define PARAC_MESSAGING_QUEUE_SIZE 20

typedef enum parac_status (*parac_messaging_queue_forward)(parac_message* msg);

typedef struct parac_messaging_queue {
  parac_message entry[PARAC_MESSAGING_QUEUE_SIZE];

  size_t cursor;
  size_t entries;

  parac_messaging_queue_forward forward;
} parac_messaging_queue;

void
parac_messaging_queue_init(parac_messaging_queue* queue);

bool
parac_messaging_queue_empty(const parac_messaging_queue* queue);

/** @brief Remove element from front of queue.
 *
 * The embedded data must be freed!
 */
parac_message*
parac_messaging_queue_pop(parac_messaging_queue* queue);

enum parac_status
parac_messaging_queue_push(parac_messaging_queue* queue, parac_message* data);

#ifdef __cplusplus
}
#endif

#endif
