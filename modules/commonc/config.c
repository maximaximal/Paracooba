#include "paracooba/common/log.h"
#include "paracooba/common/status.h"
#include "paracooba/common/types.h"
#include <paracooba/common/config.h>

#include <parac_common_export.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

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

      switch(e->type) {
        case PARAC_TYPE_STR:
          e->value.string = strdup(e->default_value.string);
          break;
        case PARAC_TYPE_VECTOR_STR:
          e->value.string_vector.size = e->default_value.string_vector.size;
          e->value.string_vector.strings =
            malloc(sizeof(const char*) * e->default_value.string_vector.size);
          for(size_t i = 0; i < e->default_value.string_vector.size; ++i)
            e->value.string_vector.strings[i] =
              strdup(e->default_value.string_vector.strings[i]);
          break;
        default:
          e->value = e->default_value;
          break;
      }
    }
  }
}

PARAC_COMMON_EXPORT void
parac_config_parse_env(parac_config* config) {
  assert(config);

  parac_config_entry_node** node = &config->first_node;

  while(*node) {
    parac_config_entry* entries = (*node)->entries;
    size_t size = (*node)->size;
    node = &(*node)->next;

    for(size_t i = 0; i < size; ++i) {
      parac_config_entry* e = &entries[i];

      assert(e);
      assert(e->name);

      size_t uppercase_name_len = strlen(e->name) + 6 + 1;
      char uppercase_name[uppercase_name_len];
      uppercase_name[uppercase_name_len - 1] = '\0';
      uppercase_name[0] = 'P';
      uppercase_name[1] = 'A';
      uppercase_name[2] = 'R';
      uppercase_name[3] = 'A';
      uppercase_name[4] = 'C';
      uppercase_name[5] = '_';
      for(size_t i = 6; i < uppercase_name_len - 1; ++i) {
        if(e->name[i - 6] == '-') {
          uppercase_name[i] = '_';
        } else {
          uppercase_name[i] = toupper(e->name[i - 6]);
        }
      }
      if(getenv(uppercase_name)) {
        bool success =
          parac_type_from_str(getenv(uppercase_name), e->type, &e->value);
        if(!success) {
          printf("c !! Could not parse environment variable %s (value \"%s\") "
                 "into config "
                 "variable %s!\n",
                 uppercase_name,
                 getenv(uppercase_name),
                 e->name);
        } else {
          // Useful for debugging, but deactivated by default.
          // printf("c Applied %s over %s to %s\n",
          //       getenv(uppercase_name),
          //       uppercase_name,
          //       e->name);
        }
      }
    }
  }
}
