#ifndef PARACOOBA_MODULE_RUNNER_H
#define PARACOOBA_MODULE_RUNNER_H

#include <paracooba/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;

typedef uint16_t (*parac_runner_get_available_worker_count)(
  struct parac_module*);
typedef uint16_t (*parac_runner_get_busy_workers)(struct parac_module*);

typedef struct parac_module_runner {
  parac_runner_get_available_worker_count get_available_worker_count;
  parac_runner_get_busy_workers get_busy_workers;
} parac_module_runner;

#ifdef __cplusplus
}
#endif

#endif
