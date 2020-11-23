#ifndef PARACOOBA_COMMON_TIMEOUT_H
#define PARACOOBA_COMMON_TIMEOUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*parac_timeout_cancel)(struct parac_timeout*);
typedef void (*parac_timeout_expired)(struct parac_timeout*);

typedef struct parac_timeout {
  void* cancel_userdata;
  parac_timeout_cancel cancel;

  void* expired_userdata;
  parac_timeout_expired expired;
} parac_timeout;

#ifdef __cplusplus
}
#endif

#endif
