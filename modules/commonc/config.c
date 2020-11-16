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
  config->first_node = NULL;
  config->userdata = NULL;
}

PARAC_COMMON_EXPORT
parac_config_entry*
parac_config_reserve(parac_config* config, size_t entry_count) {
  assert(config);

  parac_config_entry_node** node = &config->first_node;

  while(*node) {
    node = &(*node)->next;
  }

  (*node) = calloc(sizeof(parac_config_entry_node), 1);
  (*node)->entries = calloc(sizeof(parac_config_entry), entry_count);
  (*node)->size = entry_count;
  (*node)->next = NULL;
  return (*node)->entries;
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

  parac_config_entry_node* node = config->first_node;

  while(node != NULL) {
    parac_config_entry* entries = node->entries;
    size_t size = node->size;
    parac_config_entry_node* last_node = node;
    node = node->next;

    for(size_t i = 0; i < size; ++i) {
      parac_config_entry* e = &entries[i];
      if(e->name)
        free(e->name);
      if(e->description)
        free(e->description);
    }

    free(entries);
    free(last_node);
  }
  config->first_node = NULL;
}

PARAC_COMMON_EXPORT void
parac_config_apply_default_values(parac_config* config) {
  assert(config);

  parac_config_entry_node** node = &config->first_node;

  while(*node) {
    parac_config_entry* entries = (*node)->entries;
    size_t size = (*node)->size;
    node = &(*node)->next;

    for(size_t i = 0; i < size; ++i) {
      parac_config_entry* e = &entries[i];
      e->value = e->default_value;
    }
  }
}
