#include "paracooba/module.h"

extern "C" {
#ifdef PARAC_USE_STATIC_parac_broker
extern parac_status
parac_module_discover_broker(parac_handle* handle);
#endif

#ifdef PARAC_USE_STATIC_parac_runner
extern parac_status
parac_module_discover_runner(parac_handle* handle);
#endif

#if defined(PARAC_USE_STATIC_parac_solver)
extern parac_status
parac_module_discover_solver(parac_handle* handle);
#endif

#if defined(PARAC_USE_STATIC_parac_solver_qbf)
extern parac_status
parac_module_discover_solver_qbf(parac_handle* handle);
#endif

#ifdef PARAC_USE_STATIC_parac_communicator
extern parac_status
parac_module_discover_communicator(parac_handle* handle);
#endif
}

typedef parac_status (*parac_module_discover_func)(parac_handle*);

// Discover modules statically linked into the executable.
parac_module_discover_func
parac_static_module_discover(parac_module_type mod) {
  switch(mod) {
    case PARAC_MOD_BROKER:
#ifdef PARAC_USE_STATIC_parac_broker
      return &parac_module_discover_broker;
#else
      return nullptr;
#endif
    case PARAC_MOD_RUNNER:
#ifdef PARAC_USE_STATIC_parac_runner
      return &parac_module_discover_runner;
#else
      return nullptr;
#endif
    case PARAC_MOD_SOLVER:
#if defined(PARAC_USE_STATIC_parac_solver)
      return &parac_module_discover_solver;
#elif defined(PARAC_USE_STATIC_parac_solver_qbf)
      return &parac_module_discover_solver_qbf;
#else
      return nullptr;
#endif
    case PARAC_MOD_COMMUNICATOR:
#ifdef PARAC_USE_STATIC_parac_communicator
      return &parac_module_discover_communicator;
#else
      return nullptr;
#endif
    default:
      return nullptr;
  }
}
