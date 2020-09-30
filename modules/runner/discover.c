#include <parac_runner_export.h>
#include <paracooba/module.h>

extern parac_status
parac_module_discover_runner(parac_handle* handle);

PARAC_RUNNER_EXPORT parac_status
parac_module_discover(parac_handle* handle) {
  return parac_module_discover_runner(handle);
}
