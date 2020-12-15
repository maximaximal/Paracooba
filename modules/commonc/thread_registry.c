#include "paracooba/common/status.h"
#include <paracooba/common/linked_list.h>
#include <paracooba/common/thread_registry.h>

#include <parac_common_export.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#ifdef __STDC_NO_THREADS__
// C11 Threads not available, using c11threads from
// https://github.com/jtsiomb/c11thread0s
#include "c11threads.h"
#else
#include <threads.h>
#endif

static atomic_bool global_exit = false;

typedef struct thread_handle {
  parac_thread_registry_handle* registry_handle;
  thrd_t thread;
} thread_handle;

PARAC_LINKED_LIST(thread_handle, thread_handle)
PARAC_LINKED_LIST(thread_registry_new_thread_starting_cb,
                  parac_thread_registry_new_thread_starting_cb)

PARAC_COMMON_EXPORT
void
parac_thread_registry_init(parac_thread_registry* registry) {
  assert(registry);

  registry->threads = malloc(sizeof(parac_thread_handle_list));
  registry->new_thread_starting_cbs =
    malloc(sizeof(parac_thread_registry_new_thread_starting_cb_list));

  assert(registry->threads);
  assert(registry->new_thread_starting_cbs);

  parac_thread_handle_list_init(registry->threads);
  parac_thread_registry_new_thread_starting_cb_list_init(
    registry->new_thread_starting_cbs);

  atomic_store(&global_exit, false);
}

PARAC_COMMON_EXPORT
void
parac_thread_registry_free(parac_thread_registry* registry) {
  // Once the thread registry is freed, no other thread should be started! This
  // may happen in quick starts and stops.
  atomic_store(&global_exit, true);

  assert(registry);
  assert(registry->threads);
  assert(registry->new_thread_starting_cbs);

  parac_thread_handle_list_free(registry->threads);
  parac_thread_registry_new_thread_starting_cb_list_free(
    registry->new_thread_starting_cbs);

  free(registry->threads);
  free(registry->new_thread_starting_cbs);
}

static int
run_wrapper(parac_thread_registry_handle* handle) {
  int returncode = 0;

  if((atomic_bool*)atomic_load(&global_exit))
    return PARAC_PREMATURE_EXIT;

  parac_thread_registry* registry = handle->registry;
  struct parac_thread_registry_new_thread_starting_cb_list_entry* cb =
    registry->new_thread_starting_cbs->first;
  while(cb) {
    if(global_exit)
      return PARAC_PREMATURE_EXIT;
    if(cb->entry) {
      cb->entry(handle);
    }
    cb = cb->next;
  }

  handle->running = true;
  returncode = handle->start_func(handle);
  handle->running = false;
  return returncode;
}

PARAC_COMMON_EXPORT
parac_status
parac_thread_registry_create(parac_thread_registry* registry,
                             struct parac_module* starter,
                             parac_thread_registry_start_func start_func,
                             parac_thread_registry_handle* handle) {
  assert(registry);
  assert(start_func);

  thread_handle* thandle =
    parac_thread_handle_list_alloc_new(registry->threads);
  if(!thandle) {
    return PARAC_OUT_OF_MEMORY;
  }

  thandle->registry_handle = handle;

  handle->stop = false;
  handle->running = false;
  handle->thread_id = registry->threads->size;
  handle->starter = starter;
  handle->start_func = start_func;
  handle->registry = registry;

  int success =
    thrd_create(&thandle->thread, (int (*)(void*))run_wrapper, handle);
  if(success == thrd_success) {
    return PARAC_OK;
  } else if(success == thrd_nomem) {
    return PARAC_OUT_OF_MEMORY;
  } else {
    return PARAC_GENERIC_ERROR;
  }
}

PARAC_COMMON_EXPORT
parac_status
parac_thread_registry_add_starting_callback(
  parac_thread_registry* registry,
  parac_thread_registry_new_thread_starting_cb cb) {
  assert(registry);
  assert(registry->new_thread_starting_cbs);
  assert(cb);

  parac_thread_registry_new_thread_starting_cb* internal_cb =
    parac_thread_registry_new_thread_starting_cb_list_alloc_new(
      registry->new_thread_starting_cbs);
  if(!internal_cb)
    return PARAC_OUT_OF_MEMORY;
  *internal_cb = cb;
  return PARAC_OK;
}

PARAC_COMMON_EXPORT
void
parac_thread_registry_stop(parac_thread_registry* registry) {
  struct parac_thread_handle_list_entry* handle = registry->threads->first;
  while(handle) {
    handle->entry.registry_handle->stop = true;
    handle = handle->next;
  }
}

PARAC_COMMON_EXPORT
void
parac_thread_registry_wait_for_exit(parac_thread_registry* registry) {
  struct parac_thread_handle_list_entry* handle = registry->threads->first;
  while(handle) {
    thrd_join(handle->entry.thread,
              &handle->entry.registry_handle->exit_status);
    handle = handle->next;
  }
  registry->threads->first = NULL;
}
