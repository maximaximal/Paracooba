#ifndef PARAC_COMMON_CONFIG_H
#define PARAC_COMMON_CONFIG_H

#include "../module.h"
#include "status.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct parac_config_entry {
  parac_type type;
  char* name;
  char* description;
  parac_module_type registrar;
  parac_type_union value;
  parac_type_union default_value;
} parac_config_entry;

typedef struct parac_config {
  size_t size;
  size_t reserved_size;
  parac_config_entry* entries;
  parac_id id;

  void* userdata;/// Owned by Executor.
} parac_config;

void
parac_config_init(parac_config* config);

void
parac_config_free(parac_config* config);

/** @brief Reserves space in the config array and returns entries.
 */
parac_config_entry*
parac_config_reserve(parac_config* config, size_t entry_count);

void
parac_config_entry_set_str(parac_config_entry* e,
                           const char* name,
                           const char* description);

#ifdef __cplusplus
}
namespace paracooba {
struct ConfigWrapper : public parac_config {
  ConfigWrapper() { parac_config_init(this); }
  ~ConfigWrapper() { parac_config_free(this); }
};
}

inline std::ostream&
operator<<(std::ostream& o, const parac_config_entry& e) {
  ApplyFuncToParacTypeUnion(e.type, e.value, [&o](auto&& v) { o << v; });
  return o;
}
#endif

#endif
