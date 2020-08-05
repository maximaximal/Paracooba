#include "paracooba/common/status.h"
#include <paracooba/common/config.h>

#include <parac_common_export.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

PARAC_COMMON_EXPORT
void
parac_config_init(parac_config* config) {
  assert(config);
  config->id = 0;
  config->size = 0;
  config->reserved_size = 0;
  config->userdata = NULL;
  config->entries = NULL;
}

PARAC_COMMON_EXPORT
parac_status
parac_config_reserve(parac_config* config, size_t entry_count) {
  assert(config);
  assert(config->reserved_size > 0 ||
         (config->reserved_size == 0 && config->entries == NULL));

  config->entries = realloc(config->entries,
                            sizeof(parac_config_entry*) *
                              (config->reserved_size + entry_count));

  if(config->entries == NULL) {
    return PARAC_OUT_OF_MEMORY;
  } else {
    memset(config->entries + config->reserved_size,
           0,
           sizeof(parac_config_entry*) * entry_count);
    config->reserved_size += entry_count;
    return PARAC_OK;
  }
}

PARAC_COMMON_EXPORT
parac_status
parac_config_register(parac_config* config, parac_config_entry* entry) {
  assert(config);
  assert(config->entries);
  assert(entry);

  if(config->size >= config->reserved_size) {
    return PARAC_FULL;
  }

  config->entries[config->size++] = entry;

  return PARAC_OK;
}

PARAC_COMMON_EXPORT
void
parac_config_free(parac_config* config) {
  assert(config);
  if(config->entries) {
    free(config->entries);
    config->entries = 0;
    config->size = 0;
    config->reserved_size = 0;
  }
}
