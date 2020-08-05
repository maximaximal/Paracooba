#ifndef PARACOOBA_COMMON_STATUS_H
#define PARACOOBA_COMMON_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum parac_status {
  PARAC_OK,
  PARAC_FULL,
  PARAC_OUT_OF_MEMORY,
  PARAC_INVALID_CHAR_ENCOUNTERED,
  PARAC_GENERIC_ERROR
} parac_status;

const char*
parac_status_to_str(parac_status status);

#ifdef __cplusplus
}

#include <iostream>

inline std::ostream&
operator<<(std::ostream& o, parac_status status) {
  return o << parac_status_to_str(status);
}
#endif

#endif
