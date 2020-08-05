#ifndef PARAC_COMMON_CONFIG_H
#define PARAC_COMMON_CONFIG_H

#include "status.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct parac_config_entry {
  parac_type type;
  const char** names;
  struct parac_module* registrar;
  parac_type_union value;
  parac_type_union default_value;
} parac_config_entry;

typedef struct parac_config {
  size_t size;
  size_t reserved_size;
  parac_config_entry** entries;
  parac_id id;

  void* userdata;/// Owned by Executor.
} parac_config;

void
parac_config_init(parac_config* config);

void
parac_config_free(parac_config* config);

/** @brief Reserves space in the config array.
 *
 * Must be called to be able to add entries to the global config uitility.
 */
parac_status
parac_config_reserve(parac_config* config, size_t entry_count);

/** @brief Registers the given entry in the pre-reserved config array.
 *
 * May only be called after parac_config_reserve has reserved enough space!
 */
parac_status
parac_config_register(parac_config* config, parac_config_entry* entry);

#ifdef __cplusplus
}
#endif

#endif
