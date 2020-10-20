#ifndef PARACOOBA_MODULE_RUNNER_H
#define PARACOOBA_MODULE_RUNNER_H

#include <paracooba/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct parac_module_runner {
  uint16_t available_worker_count;
  uint16_t busy_worker_count;
} parac_module_runner;

#ifdef __cplusplus
}
#endif

#endif
