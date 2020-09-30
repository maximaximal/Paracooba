#include <parac_broker_export.h>
#include <paracooba/module.h>

extern parac_status
parac_module_discover_broker(parac_handle* handle);

PARAC_BROKER_EXPORT parac_status
parac_module_discover(parac_handle* handle) {
  return parac_module_discover_broker(handle);
}
