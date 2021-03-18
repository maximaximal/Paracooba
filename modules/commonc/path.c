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
  assert(!parac_path_is_overlength(p));
  parac_path_type path = p.path_;
  return path << PARAC_PATH_LENGTH_BITS;
}

PARAC_COMMON_EXPORT bool
parac_path_get_assignment(parac_path p, parac_path_length_type d) {
  assert(!parac_path_is_overlength(p));
  assert(d <= PARAC_PATH_MAX_LENGTH);
  assert(d >= 1);
  return p.rep & (long1 << ((sizeof(parac_path) * 8) - 1 - (d - 1)));
}

PARAC_COMMON_EXPORT parac_path
parac_path_set_assignment(parac_path p, parac_path_length_type d, bool v) {
  assert(!parac_path_is_overlength(p));
  assert(d >= 1);
  assert(d <= PARAC_PATH_MAX_LENGTH);

  p.rep = (p.rep & ~(long1 << ((sizeof(parac_path) * 8) - 1 - (d - 1)))) |
          (((parac_path_type)v) << ((sizeof(parac_path) * 8) - 1 - (d - 1)));
  return p;
}

PARAC_COMMON_EXPORT bool
parac_path_is_root(parac_path p) {
  return p.length_ == 0;
}

PARAC_COMMON_EXPORT parac_path_type
parac_path_get_shifted(parac_path p) {
  assert(!parac_path_is_overlength(p));
  return p.rep >> 6u;
}

PARAC_COMMON_EXPORT parac_path_type
parac_path_get_depth_shifted(parac_path p) {
  assert(!parac_path_is_overlength(p));
  assert(p.length_ <= PARAC_PATH_MAX_LENGTH);
  if(p.length_ == 0)
    return 0;
  return p.rep >> ((sizeof(p) * 8) - p.length_);
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_parent(parac_path p) {
  assert(!parac_path_is_overlength(p));
  assert(p.length_ >= 1);
  --p.length_;
  parac_path_cleanup(p);
  return p;
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_sibling(parac_path p) {
  assert(!parac_path_is_overlength(p));
  return parac_path_set_assignment(
    p, p.length_, !parac_path_get_assignment(p, p.length_));
}

static parac_path
parac_path_get_next(parac_path p, int n) {
  assert(p.rep != PARAC_PATH_EXPLICITLY_UNKNOWN);

  if(parac_path_is_overlength(p)) {
    ++p.overlength_length_;
    p.overlength_left_right = n;
  } else {
    assert(p.length_ <= PARAC_PATH_MAX_LENGTH);
    parac_path_length_type depth = p.length_ + 1;
    if(depth == PARAC_PATH_MAX_LENGTH + 1) {
      p.overlength_tag_ = PARAC_PATH_OVERLENGTH;
      p.overlength_length_ = depth;
      p.overlength_left_right = n;
    } else {
      p = parac_path_set_assignment(p, depth, n);
      p.length_ = depth;
    }
  }
  return p;
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_next_left(parac_path p) {
  return parac_path_get_next(p, 0);
}

PARAC_COMMON_EXPORT parac_path
parac_path_get_next_right(parac_path p) {
  return parac_path_get_next(p, 1);
}

PARAC_COMMON_EXPORT parac_path
parac_path_cleanup(parac_path p) {
  if(!parac_path_is_overlength(p)) {
    parac_path_length_type l = p.length_;
    p.rep &= ~((parac_path_type)0xFFFFFFFFFFFFFFFFu >> l);
    p.length_ = l;
  }
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
  } else if(p.length_ == PARAC_PATH_EXPLICITLY_UNKNOWN) {
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
  } else if(p.length_ == PARAC_PATH_PARSER) {
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
  } else if(parac_path_is_overlength(p)) {
    size_t i = snprintf(out_str,
                        PARAC_PATH_MAX_LENGTH,
                        "(overlength | %lu | l/r %u)",
                        p.overlength_length_,
                        p.overlength_left_right);
    out_str[i] = '\0';
    return;
  } else if(p.length_ > PARAC_PATH_MAX_LENGTH) {
    size_t i = 0;
    out_str[i++] = '(';
    out_str[i++] = 'i';
    out_str[i++] = 'n';
    out_str[i++] = 'v';
    out_str[i++] = 'a';
    out_str[i++] = 'l';
    out_str[i++] = 'i';
    out_str[i++] = 'd';
    out_str[i++] = ' ';
    out_str[i++] = 'p';
    out_str[i++] = 'a';
    out_str[i++] = 't';
    out_str[i++] = 'h';
    out_str[i++] = ')';
    out_str[i++] = '\0';
    return ;
  }

  for(size_t i = 0; i < PARAC_PATH_MAX_LENGTH; ++i) {
    out_str[i] = parac_path_get_assignment(p, i + 1) + '0';
  }
  out_str[p.length_] = '\0';
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

  p.length_ = len;
  *tgt = p;

  return PARAC_OK;
}

PARAC_COMMON_EXPORT parac_path
parac_path_unknown() {
  parac_path p;
  p.rep = PARAC_PATH_EXPLICITLY_UNKNOWN;
  return p;
}

PARAC_COMMON_EXPORT parac_path
parac_path_root() {
  parac_path p;
  p.length_ = 0;
  p.path_ = 0;
  return p;
}

PARAC_COMMON_EXPORT bool
parac_path_is_overlength(parac_path p) {
  return p.overlength_tag_ == PARAC_PATH_OVERLENGTH;
}

PARAC_COMMON_EXPORT parac_path_length_type
parac_path_length(parac_path p) {
  if(parac_path_is_overlength(p)) {
    return p.overlength_length_;
  } else {
    return p.length_;
  }
}
