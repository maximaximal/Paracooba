#include <paracooba/common/message.h>

#include <parac_common_export.h>

#include <assert.h>
#include <stdlib.h>

PARAC_COMMON_EXPORT
void
parac_message_free(parac_message* msg) {
  assert(msg);
  if(msg->data_to_be_freed) {
    assert(msg->data);
    free((void*)msg->data);
    msg->data = NULL;
  }
}
