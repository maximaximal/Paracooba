#ifndef PARACOOBA_COMMON_PATH_H
#define PARACOOBA_COMMON_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PARAC_PATH_MAX_LENGTH 58

typedef uint64_t parac_path_type;
typedef uint8_t parac_path_length_type;

typedef struct parac_path {
  union {
    struct __attribute__((__packed__)) {
      parac_path_type path : 58;
      parac_path_length_type length : 6;
    };
    parac_path_type rep;
  };
} parac_path;

bool
parac_path_get_assignment(parac_path p, parac_path_length_type d);

parac_path
parac_path_set_assignment(parac_path p, parac_path_length_type d, bool v);

parac_path_type
parac_path_get_depth_shifted(parac_path p);

parac_path_type
parac_path_get_shifted(parac_path p);

parac_path
parac_path_get_parent(parac_path p);

parac_path
parac_path_get_sibling(parac_path p);

parac_path
parac_path_get_next_left(parac_path p);

parac_path
parac_path_get_next_right(parac_path p);

parac_path
parac_path_cleanup(parac_path p);

void
parac_path_to_str(parac_path p, char* out_str);

#ifdef __cplusplus
}
#include <iostream>

bool operator[](parac_path p, parac_path_leparac_path_length_type d) {
  return parac_path_get_assparac_path_get_assignment(p, d);
}

std::ostream&
operator<<(std::ostream& o, parac_path p) {
  char str[PARAC_PATH_MAX_LENGTH];
  parac_path_to_sparac_path_to_str(p, str);
  return o << str;
}
#endif

#endif
