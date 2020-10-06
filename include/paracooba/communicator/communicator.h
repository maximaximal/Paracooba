#ifndef PARACOOBA_MODULE_COMMUNICATOR_H
#define PARACOOBA_MODULE_COMMUNICATOR_H

#include <paracooba/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;

typedef struct parac_module_communicator {
  // The communicator module works solely by enriching other modules and calling
  // their functions. It does not need any public functions on its own.
  int _empty;
} parac_module_communicator;

#ifdef __cplusplus
}
#endif

#endif
