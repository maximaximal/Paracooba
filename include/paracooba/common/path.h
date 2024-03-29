#ifndef PARACOOBA_COMMON_PATH_H
#define PARACOOBA_COMMON_PATH_H

#include "paracooba/common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PARAC_PATH_MAX_LENGTH 58
#define PARAC_PATH_STR_LENGTH (PARAC_PATH_MAX_LENGTH + 1)
#define PARAC_PATH_EXPLICITLY_UNKNOWN 0b00111110u
#define PARAC_PATH_PARSER 0b00111101u
#define PARAC_PATH_OVERLENGTH 0b00111011u
#define PARAC_PATH_EXTENDED 0b00111100
#define PARAC_PATH_BITS 58u
#define PARAC_PATH_LENGTH_BITS 6u

typedef uint64_t parac_path_type;
typedef uint64_t parac_path_length_type;

typedef struct __attribute__((__packed__)) parac_path {
  union __attribute__((__packed__)) {
    struct __attribute__((__packed__)) {
      parac_path_length_type length_ : 6;
      parac_path_type path_ : 58;
    };
    struct __attribute__((__packed__)) {
      parac_path_length_type overlength_tag_ : 6;
      parac_path_type overlength_length_ : 57;
      uint8_t overlength_left_right : 1;
    };
    parac_path_type rep;
  };

#ifdef __cplusplus
  bool operator==(const parac_path& p) const { return rep == p.rep; }
#endif
} parac_path;

parac_path_type
parac_path_left_aligned(parac_path p);

bool
parac_path_get_assignment(parac_path p, parac_path_length_type d);

bool
parac_path_is_root(parac_path p);

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
parac_path_get_next_extended(parac_path p);

parac_path
parac_path_cleanup(parac_path p);

parac_status
parac_path_from_str(const char* str, size_t len, parac_path* tgt);

void
parac_path_to_str(parac_path p, char* out_str);

void
parac_path_print(parac_path p);

parac_path
parac_path_unknown();

parac_path
parac_path_root();

bool
parac_path_is_overlength(parac_path p);

bool
parac_path_is_extended(parac_path p);

parac_path_length_type
parac_path_length(parac_path p);

#ifdef __cplusplus
}
#include <iostream>
#include <string>
#include <type_traits>

template<typename T>
parac_path
parac_path_build(T p, parac_path_length_type l) {
  assert(l <= PARAC_PATH_MAX_LENGTH);
  parac_path path;
  path.rep = p;
  path.rep <<= (((sizeof(parac_path)) - (sizeof(T))) * 8);
  path.length_ = l;
  path = parac_path_cleanup(path);
  return path;
}

namespace parac {
class Path : public parac_path {

  public:
  using type = parac_path_type;
  using length_type = parac_path_length_type;

  inline Path() = default;
  inline Path(const Path& o) = default;
  inline Path(parac_path p) { rep = p.rep; }
  inline Path(parac_path_type p) { rep = p; }

  template<typename T>
  inline static Path build(T p, length_type l) {
    return parac_path_build(p, l);
  }

  inline Path left() const { return parac_path_get_next_left(*this); }
  inline Path right() const { return parac_path_get_next_right(*this); }
  inline Path next_extended() const {
    return parac_path_get_next_extended(*this);
  }
  inline Path parent() const { return parac_path_get_parent(*this); }
  inline Path sibling() const { return parac_path_get_sibling(*this); }
  inline type shifted() const { return parac_path_get_shifted(*this); }
  inline type depth_shifted() const {
    return parac_path_get_depth_shifted(*this);
  }
  inline type left_aligned() const { return parac_path_left_aligned(*this); }

  struct BoolWrapper {
    public:
    inline BoolWrapper(Path& path, length_type pos)
      : m_path(path)
      , m_pos(pos) {}
    inline operator bool() const {
      return parac_path_get_assignment(m_path, m_pos);
    }
    inline void operator=(bool val) {
      m_path = parac_path_set_assignment(m_path, m_pos, val);
    }

    private:
    Path& m_path;
    length_type m_pos;
  };

  inline BoolWrapper operator[](parac_path_length_type pos) {
    return BoolWrapper(*this, pos);
  }
  inline bool operator[](parac_path_length_type pos) const {
    return parac_path_get_assignment(*this, pos);
  }

  inline parac_path_length_type length() const {
    return parac_path_length(*this);
  }
  inline void operator=(parac_path p) { rep = p.rep; }
  inline bool operator==(const parac_path& p) const { return rep == p.rep; }
  inline operator uint64_t() const { return rep; }
  inline bool operator<(const Path& o) const { return length() < o.length(); }
};

inline std::string
to_string(Path const& p) {
  char str[PARAC_PATH_STR_LENGTH];
  parac_path_to_str(p, str);
  return std::string(str);
}
}

inline std::ostream&
operator<<(std::ostream& o, parac_path p) {
  char str[PARAC_PATH_STR_LENGTH];
  parac_path_to_str(p, str);
  return o << str;
}
inline std::ostream&
operator<<(std::ostream& o, parac::Path p) {
  char str[PARAC_PATH_STR_LENGTH];
  parac_path_to_str(p, str);
  return o << str;
}
#endif

#endif
