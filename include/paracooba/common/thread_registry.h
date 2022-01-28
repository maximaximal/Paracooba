#ifndef PARAC_COMMON_THREAD_REGISTRY_H
#define PARAC_COMMON_THREAD_REGISTRY_H

#include "status.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct parac_thread_regitstry;
struct parac_thread_registry_handle;

typedef int (*parac_thread_registry_start_func)(
  struct parac_thread_registry_handle*);

typedef void (*parac_thread_registry_new_thread_starting_cb)(
  struct parac_thread_registry_handle*);

typedef void (*parac_thread_registry_stop_notifier)(
  struct parac_thread_registry_handle*);

typedef struct parac_thread_registry {
  struct parac_thread_handle_list* threads;
  struct parac_thread_registry_new_thread_starting_cb_list*
    new_thread_starting_cbs;
  parac_id belongs_to_id;
} parac_thread_registry;

typedef struct parac_thread_handle parac_thread_handle;

typedef struct parac_thread_registry_handle {
  uint16_t thread_id;
  bool stop;
  bool running;
  int exit_status;// Should be of type parac_status
  struct parac_module* starter;
  void* userdata;
  parac_thread_registry* registry;
  parac_thread_handle* thread_handle;
  parac_thread_registry_start_func start_func;
  parac_thread_registry_stop_notifier stop_notifier;
} parac_thread_registry_handle;

void
parac_thread_registry_init(parac_thread_registry* registry,
                           parac_id belongs_to_id);

void
parac_thread_registry_free(parac_thread_registry* registry);

parac_status
parac_thread_registry_create(parac_thread_registry* registry,
                             struct parac_module* starter,
                             parac_thread_registry_start_func start_func,
                             parac_thread_registry_handle* handle);

parac_status
parac_thread_registry_add_starting_callback(
  parac_thread_registry* registry,
  parac_thread_registry_new_thread_starting_cb cb);

void
parac_thread_registry_stop(parac_thread_registry* registry);

void
parac_thread_registry_wait_for_exit(parac_thread_registry* registry);

void
parac_thread_registry_wait_for_exit_of_thread(
  parac_thread_registry_handle* handle);

#ifdef __cplusplus
}

namespace paracooba {
struct ThreadRegistryWrapper : public parac_thread_registry {
  ThreadRegistryWrapper(parac_id belongs_to_id) {
    parac_thread_registry_init(this, belongs_to_id);
  }
  ~ThreadRegistryWrapper() { parac_thread_registry_free(this); }

  void wait() { parac_thread_registry_wait_for_exit(this); }
};
}
#endif

#endif
