#include <paracooba/module.h>

#include <parac_modules_export.h>

PARAC_MODULES_EXPORT const char*
parac_module_type_to_str(parac_module_type type) {
  switch(type) {
    case PARAC_MOD_BROKER:
      return "broker";
    case PARAC_MOD_RUNNER:
      return "runner";
    case PARAC_MOD_COMMUNICATOR:
      return "communicator";
    case PARAC_MOD_SOLVER:
      return "solver";
    case PARAC_MOD__COUNT:
      break;
  }
  return "!";
}
