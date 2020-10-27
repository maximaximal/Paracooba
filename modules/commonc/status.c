#include <paracooba/common/status.h>

#include <parac_common_export.h>

PARAC_COMMON_EXPORT const char*
parac_status_to_str(parac_status status) {
  switch(status) {
    case PARAC_OK:
      return "Ok";
    case PARAC_FULL:
      return "Full";
    case PARAC_PENDING:
      return "Pending";
    case PARAC_UNDEFINED:
      return "Undefined";
    case PARAC_TO_BE_DELETED:
      return "To be deleted";
    case PARAC_CONNECTION_CLOSED:
      return "Connection Closed";
    case PARAC_OUT_OF_MEMORY:
      return "Out of Memory";
    case PARAC_INVALID_CHAR_ENCOUNTERED:
      return "Invalid Char Encountered";
    case PARAC_INVALID_IP:
      return "Provided IP address could not be parsed";
    case PARAC_PARSE_ERROR:
      return "Parse Error";
    case PARAC_GENERIC_ERROR:
      return "Generic Error";
  }
}
