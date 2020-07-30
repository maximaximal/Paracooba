#include <assert.h>
#include <stdint.h>

#include <paracooba/common/path.h>

static_assert(sizeof(parac_path) == sizeof(parac_path_type),
              "Paracooba paths must be representable as primitives.");

bool
parac_path_get_assignment(parac_path p, parac_path_length_type d) {
  assert(d <= PARAC_PATH_MAX_LENGTH);
  return p.path & ((1u) << ((sizeof(parac_path) * 8) - 1 - (d - 1)));
}

parac_path
parac_path_set_assignment(parac_path p, parac_path_length_type d, bool v) {
  assert(d >= 1);
  assert(d <= PARAC_PATH_MAX_LENGTH);

  p.path &= ~(1u << ((sizeof(parac_path) * 8) - 1 - (d - 1))) |
            (v << ((sizeof(parac_path) * 8) - 1 - (d - 1)));
  return p;
}

parac_path_type
parac_path_get_shifted(parac_path p) {
  return p.rep >> 6u;
}

parac_path_type
parac_path_get_depth_shifted(parac_path p) {
  assert(p.length <= PARAC_PATH_MAX_LENGTH);
  if(p.length == 0)
    return 0;
  return p.rep >> ((sizeof(p) * 8) - p.length);
}

parac_path
parac_path_get_parent(parac_path p) {
  assert(p.length >= 1);
  --p.length;
  parac_path_cleanup(p);
  return p;
}

parac_path
parac_path_get_sibling(parac_path p) {
  return parac_path_set_assignment(
    p, p.length, !parac_path_get_assignment(p, p.length));
}

parac_path
parac_path_get_next_left(parac_path p) {
  parac_path_length_type depth = p.length + 1;
  assert(depth <= PARAC_PATH_MAX_LENGTH);
  p = parac_path_set_assignment(p, depth, false);
  p.length = depth;
  return p;
}

parac_path
parac_path_get_next_right(parac_path p) {
  parac_path_length_type depth = p.length + 1;
  assert(depth <= PARAC_PATH_MAX_LENGTH);
  p = parac_path_set_assignment(p, depth, true);
  p.length = depth;
  return p;
}

parac_path
parac_path_cleanup(parac_path p) {
  parac_path_length_type l = p.length;
  p.rep &= ~(0xFFFFFFFFFFFFFFFFu >> l);
  p.length = l;
  return p;
}

void
parac_path_to_str(parac_path p, char* out_str) {
  if(p.length == 0) {
    size_t i = 0;
    out_str[i++] = '(';
    out_str[i++] = 'r';
    out_str[i++] = 'o';
    out_str[i++] = 'o';
    out_str[i++] = 't';
    out_str[i++] = ')';
    out_str[i++] = '\n';
    return;
  }
  if(p.length >= PARAC_PATH_MAX_LENGTH) {
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
    out_str[i++] = '\n';
    return;
  }

  for(size_t i = 0; i < PARAC_PATH_MAX_LENGTH; ++i) {
    out_str[i] = parac_path_get_assignment(p, i + 1) + '0';
  }
  out_str[p.length] = '\0';
}
