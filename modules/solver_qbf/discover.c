#include <parac_solver_qbf_export.h>
#include <paracooba/module.h>

extern parac_status
parac_module_discover_solver_qbf(parac_handle* handle);

PARAC_SOLVER_QBF_EXPORT parac_status
parac_module_discover(parac_handle* handle) {
  return parac_module_discover_solver_qbf(handle);
}
