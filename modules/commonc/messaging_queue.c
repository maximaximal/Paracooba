#include <assert.h>

#include <paracooba/common/messaging_queue.h>
#include <paracooba/common/status.h>

static void
cursorpp(parac_messaging_queue* queue) {
  assert(queue);
  ++queue->cursor;
  if(queue->cursor >= PARAC_MESSAGING_QUEUE_SIZE) {
    queue->cursor = 0;
  }
}

void
parac_messaging_queue_init(parac_messaging_queue* queue) {
  assert(queue);
  for(size_t i = 0; i < PARAC_MESSAGING_QUEUE_SIZE; ++i) {
    queue->entry[i].data = NULL;
  }
  queue->cursor = 0;
  queue->entries = 0;
  queue->forward = NULL;
}

bool
parac_messaging_queue_empty(const parac_messaging_queue* queue) {
  assert(queue);
  return queue->entries == 0;
}

parac_message*
parac_messaging_queue_pop(parac_messaging_queue* queue) {
  assert(queue);
  if(parac_messaging_queue_empty(queue)) {
    return NULL;
  }

  assert(queue->entries < PARAC_MESSAGING_QUEUE_SIZE);

  parac_message* entry = &queue->entry[queue->cursor];

  cursorpp(queue);

  return entry;
}

parac_status
parac_messaging_queue_push(parac_messaging_queue* queue, parac_message* data) {
  assert(queue);

  if(queue->forward) {
    queue->forward(data);
  }

  if(queue->entries >= PARAC_MESSAGING_QUEUE_SIZE - 1) {
    return PARAC_QUEUE_FULL;
  }

  size_t i = (queue->cursor + queue->entries) % PARAC_MESSAGING_QUEUE_SIZE;
  queue->entry[i] = *data;

  ++queue->entries;

  return PARAC_OK;
}
