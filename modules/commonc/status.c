#include <paracooba/common/status.h>

#include <parac_common_export.h>

PARAC_COMMON_EXPORT const char*
parac_status_to_str(parac_status status) {
  switch(status) {
    case PARAC_OK:
      return "Ok";
    case PARAC_QUEUE_FULL:
      return "Queue Full";
    case PARAC_INVALID_CHAR_ENCOUNTERED:
      return "Invalid Char Encountered";
    case PARAC_GENERIC_ERROR:
      return "Generic Error";
  }
}
