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
parac_config_entry*
parac_config_reserve(parac_config* config, size_t entry_count) {
  assert(config);
  assert(config->reserved_size > 0 ||
         (config->reserved_size == 0 && config->entries == NULL));

  config->entries =
    realloc(config->entries,
            sizeof(parac_config_entry) * (config->reserved_size + entry_count));

  if(config->entries == NULL) {
    return NULL;
  } else {
    memset(config->entries + config->reserved_size,
           0,
           sizeof(parac_config_entry) * entry_count);

    parac_config_entry* e = config->entries + config->reserved_size;

    config->reserved_size += entry_count;
    config->size += entry_count;

    return e;
  }
}

PARAC_COMMON_EXPORT
void
parac_config_entry_set_str(parac_config_entry* e,
                           const char* name,
                           const char* description) {
  assert(e);
  assert(name);
  assert(description);
  assert(!e->name);
  assert(!e->description);

  size_t name_len = strlen(name);
  size_t desc_len = strlen(description);

  e->name = malloc(sizeof(char) * name_len + 1);
  e->description = malloc(sizeof(char) * desc_len + 1);

  strcpy(e->name, name);
  strcpy(e->description, description);
}

PARAC_COMMON_EXPORT
void
parac_config_free(parac_config* config) {
  assert(config);
  if(config->entries) {
    for(size_t i = 0; i < config->reserved_size; ++i) {
      parac_config_entry* e = &config->entries[i];
      if(e->name)
        free(e->name);
      if(e->description)
        free(e->description);
    }

    free(config->entries);
    config->entries = 0;
    config->size = 0;
    config->reserved_size = 0;
  }
}
