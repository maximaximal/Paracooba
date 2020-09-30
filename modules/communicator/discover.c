#include <parac_communicator_export.h>
#include <paracooba/module.h>

extern parac_status
parac_module_discover_communicator(parac_handle* handle);

PARAC_COMMUNICATOR_EXPORT parac_status
parac_module_discover(parac_handle* handle) {
  return parac_module_discover_communicator(handle);
}
