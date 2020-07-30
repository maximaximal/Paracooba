#ifndef PARAC_COMMON_MESSAGE_H
#define PARAC_COMMON_MESSAGE_H

#include "message_kind.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct parac_message {
  parac_message_kind kind;
  const char* data;
  size_t length;
} parac_message;

#ifdef __cplusplus
}
#endif

#endif
