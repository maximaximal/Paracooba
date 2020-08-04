#ifndef PARAC_COMMON_MESSAGE_H
#define PARAC_COMMON_MESSAGE_H

#include "message_kind.h"
#include "paracooba/common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef void(parac_message_cb)(void*, parac_status);

typedef struct parac_message {
  parac_message_kind kind;
  const char* data;
  bool data_to_be_freed;
  size_t length;
  void* userdata;
} parac_message;

/** @brief Free data in messages, if boolean flag is set.
 *
 * Also safe to call if not set.*/
void
parac_message_free(parac_message* msg);

#ifdef __cplusplus
}
#endif

#endif
