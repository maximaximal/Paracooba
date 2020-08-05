#include <paracooba/common/linked_list.h>
#include <paracooba/common/thread_registry.h>

#include <parac_common_export.h>

#include <assert.h>
#include <stdlib.h>

#ifdef __STDC_NO_THREADS__
// C11 Threads not available, using c11threads from
// https://github.com/jtsiomb/c11thread0s
#include "c11threads.h"
#else
#include <threads.h>
#endif

PARAC_LINKED_LIST(thread_registry_handle, parac_thread_registry_handle)
PARAC_LINKED_LIST(thread_registry_new_thread_starting_cb,
                  parac_thread_registry_new_thread_starting_cb)

PARAC_COMMON_EXPORT
void
parac_thread_registry_init(parac_thread_registry* registry) {
  assert(registry);

  registry->threads = malloc(sizeof(parac_thread_registry_handle_list));
  registry->new_thread_starting_cbs =
    malloc(sizeof(parac_thread_registry_new_thread_starting_cb_list));

  assert(registry->threads);
  assert(registry->new_thread_starting_cbs);

  parac_thread_registry_handle_list_init(registry->threads);
  parac_thread_registry_new_thread_starting_cb_list_init(
    registry->new_thread_starting_cbs);
}

PARAC_COMMON_EXPORT
void
parac_thread_registry_free(parac_thread_registry* registry) {
  assert(registry);
  assert(registry->threads);
  assert(registry->new_thread_starting_cbs);

  parac_thread_registry_handle_list_free(registry->threads);
  parac_thread_registry_new_thread_starting_cb_list_free(
    registry->new_thread_starting_cbs);

  free(registry->threads);
  free(registry->new_thread_starting_cbs);
}

PARAC_COMMON_EXPORT
parac_status
parac_thread_registry_create(parac_thread_registry* registry,
                             struct parac_module* starter,
                             parac_thread_registry_start_func start_func) {}

PARAC_COMMON_EXPORT
void
parac_thread_registry_stop(parac_thread_registry* registry) {}

PARAC_COMMON_EXPORT
void
parac_thread_registry_wait_for_exit(parac_thread_registry* registry) {}
