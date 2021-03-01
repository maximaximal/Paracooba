#include "paracooba/common/status.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <paracooba/common/path.h>

#include <parac_common_export.h>

static_assert(sizeof(parac_path) == sizeof(parac_path_type),
              "Paracooba paths must be representable as primitives.");

static const parac_path_type long1 = 1u;

PARAC_COMMON_EXPORT parac_path_type
parac_path_left_aligned(parac_path p) {
  parac_path_type path = p.path;
  return path << PARAC_PATH_LENGTH_BITS;
}

PARAC_COMMON_EXPORT bool
parac_path_get_assignment(parac_path p, parac_path_length_type d) {
  assert(d <= PARAC_PATH_MAX_LENGTH);
  assert(d >= 1);
  return p.rep & (long1 << ((sizeof(parac_path) * 8) - 1 - (d - 1)));
}

PARAC_COMMON_EXPORT parac_path
parac_path_set_assignment(parac_path p, parac_path_length_type d, bool v) {
  assert(d >= 1);
  assert(d <= PARAC_PATH_MAX_LENGTH);

  p.rep = (p.rep & ~(long1 << ((sizeof(parac_path) * 8) - 1 - (d - 1)))) |
          (((parac_path_type)v) << ((sizeof(parac_path) * 8) - 1 - (d - 1)));
  return p;
}

PARAC_COMMON_EXPORT bool
parac_path_is_root(parac_path p) {
  return p.length == 0;
}

PARAC_COMMON_EXPORT parac_path_type
parac_path_get_shifted(parac_path p) {
  return p.rep >> 6u;
}

PARAC_COMMON_EXPORT parac_path_type
parac_path_get_depth_shifted(parac_path p) {
  assert(p.length <= PARAC_PATH_MAX_LENGTH);
  if(p.length == 0)
    return 0;
  return p.rep >> ((sizeof(p) * 8) - p.length);
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_parent(parac_path p) {
  assert(p.length >= 1);
  --p.length;
  parac_path_cleanup(p);
  return p;
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_sibling(parac_path p) {
  return parac_path_set_assignment(
    p, p.length, !parac_path_get_assignment(p, p.length));
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_next_left(parac_path p) {
  parac_path_length_type depth = p.length + 1;
  assert(p.rep != PARAC_PATH_EXPLICITLY_UNKNOWN);
  assert(depth <= PARAC_PATH_MAX_LENGTH);
  p = parac_path_set_assignment(p, depth, false);
  p.length = depth;
  return p;
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_next_right(parac_path p) {
  parac_path_length_type depth = p.length + 1;
  assert(p.rep != PARAC_PATH_EXPLICITLY_UNKNOWN);
  assert(depth <= PARAC_PATH_MAX_LENGTH);
  p = parac_path_set_assignment(p, depth, true);
  p.length = depth;
  return p;
}

PARAC_COMMON_EXPORT parac_path
parac_path_cleanup(parac_path p) {
  parac_path_length_type l = p.length;
  p.rep &= ~((parac_path_type)0xFFFFFFFFFFFFFFFFu >> l);
  p.length = l;
  return p;
}

PARAC_COMMON_EXPORT void
parac_path_to_str(parac_path p, char* out_str) {
  if(parac_path_is_root(p)) {
    size_t i = 0;
    out_str[i++] = '(';
    out_str[i++] = 'r';
    out_str[i++] = 'o';
    out_str[i++] = 'o';
    out_str[i++] = 't';
    out_str[i++] = ')';
    out_str[i++] = '\0';
    return;
  } else if(p.length == PARAC_PATH_EXPLICITLY_UNKNOWN) {
    size_t i = 0;
    out_str[i++] = '(';
    out_str[i++] = 'e';
    out_str[i++] = 'x';
    out_str[i++] = 'p';
    out_str[i++] = 'l';
    out_str[i++] = 'i';
    out_str[i++] = 'c';
    out_str[i++] = 'i';
    out_str[i++] = 't';
    out_str[i++] = 'l';
    out_str[i++] = 'y';
    out_str[i++] = ' ';
    out_str[i++] = 'u';
    out_str[i++] = 'n';
    out_str[i++] = 'k';
    out_str[i++] = 'n';
    out_str[i++] = 'o';
    out_str[i++] = 'w';
    out_str[i++] = 'n';
    out_str[i++] = ')';
    out_str[i++] = '\0';
    return;
  } else if(p.length == PARAC_PATH_PARSER) {
    size_t i = 0;
    out_str[i++] = '(';
    out_str[i++] = 'p';
    out_str[i++] = 'a';
    out_str[i++] = 'r';
    out_str[i++] = 's';
    out_str[i++] = 'e';
    out_str[i++] = 'r';
    out_str[i++] = ')';
    out_str[i++] = '\0';
    return;
  } else if(p.length >= PARAC_PATH_MAX_LENGTH) {
    size_t i = 0;
    out_str[i++] = 'I';
    out_str[i++] = 'N';
    out_str[i++] = 'V';
    out_str[i++] = 'A';
    out_str[i++] = 'L';
    out_str[i++] = 'I';
    out_str[i++] = 'D';
    out_str[i++] = ' ';
    out_str[i++] = 'P';
    out_str[i++] = 'A';
    out_str[i++] = 'T';
    out_str[i++] = 'H';
    out_str[i++] = '\0';
    return;
  }

  for(size_t i = 0; i < PARAC_PATH_MAX_LENGTH; ++i) {
    out_str[i] = parac_path_get_assignment(p, i + 1) + '0';
  }
  out_str[p.length] = '\0';
}

PARAC_COMMON_EXPORT void
parac_path_print(parac_path p) {
  char buf[PARAC_PATH_MAX_LENGTH];
  parac_path_to_str(p, buf);
  printf("%s\n", buf);
}

parac_status
parac_path_from_str(const char* str, size_t len, parac_path* tgt) {
  parac_path p = *tgt;

  for(size_t i = 0; i < len; ++i) {
    if(str[i] == '0') {
      p = parac_path_set_assignment(p, i + 1, false);
    } else if(str[i] == '1') {
      p = parac_path_set_assignment(p, i + 1, true);
    } else {
      return PARAC_INVALID_CHAR_ENCOUNTERED;
    }
  }

  p.length = len;
  *tgt = p;

  return PARAC_OK;
}

PARAC_COMMON_EXPORT parac_path
parac_path_unknown() {
  parac_path p;
  p.rep = PARAC_PATH_EXPLICITLY_UNKNOWN;
  return p;
}
