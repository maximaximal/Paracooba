#ifndef PARAC_COMMON_MESSAGE_H
#define PARAC_COMMON_MESSAGE_H

#include "message_kind.h"
#include "status.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

struct parac_message;

typedef void (*parac_message_cb)(struct parac_message*, void*, parac_status);

typedef struct parac_message {
  parac_message_kind kind;
  char* data;
  bool data_to_be_freed;
  size_t length;
  parac_id origin;
  void* userdata;
  parac_message_cb cb;
} parac_message;

#ifdef __cplusplus
}

class parac_message_wrapper : public parac_message {
  public:
  parac_message_wrapper() {
    kind = static_cast<parac_message_kind>(0);
    data = nullptr;
    length = 0;
    data_to_be_freed = false;
    userdata = 0;
    cb = nullptr;
  };
  parac_message_wrapper(const parac_message& msg) {
    kind = msg.kind;
    data = msg.data;
    length = msg.length;
    data_to_be_freed = msg.data_to_be_freed;
    userdata = msg.userdata;
    cb = msg.cb;
  }
  ~parac_message_wrapper() { doCB(PARAC_TO_BE_DELETED); }

  void doCB(parac_status status) { cb(this, userdata, status); }
};
#endif

#endif
