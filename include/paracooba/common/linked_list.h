#ifndef PARAC_LINKED_LIST

#define PARAC_LINKED_LIST(NAME, ENTRY_TYPE)                         \
  struct parac_##NAME##_list_entry {                                \
    ENTRY_TYPE entry;                                               \
    struct parac_##NAME##_list_entry* next;                         \
  };                                                                \
  typedef struct parac_##NAME##_list {                              \
    struct parac_##NAME##_list_entry* first;                        \
    size_t size;                                                    \
  } parac_##NAME##_list;                                            \
  static void parac_##NAME##_list_init(parac_##NAME##_list* list) { \
    assert(list);                                                   \
    list->first = NULL;                                             \
    list->size = 0;                                                 \
  }                                                                 \
  static ENTRY_TYPE* parac_##NAME##_list_alloc_new(                 \
    parac_##NAME##_list* list) {                                    \
    assert(list);                                                   \
    struct parac_##NAME##_list_entry* new_entry =                   \
      (struct parac_##NAME##_list_entry*)calloc(                    \
        sizeof(struct parac_##NAME##_list_entry), 1);               \
    if(new_entry == NULL)                                           \
      return NULL;                                                  \
    new_entry->next = list->first;                                  \
    list->first = new_entry;                                        \
    ++list->size;                                                   \
    return &new_entry->entry;                                       \
  }                                                                 \
  static void parac_##NAME##_list_free(parac_##NAME##_list* list) { \
    assert(list);                                                   \
    struct parac_##NAME##_list_entry *e = list->first, *tmp;        \
    while(e) {                                                      \
      tmp = e;                                                      \
      e = e->next;                                                  \
      free(tmp);                                                    \
    }                                                               \
    list->first = NULL;                                             \
    list->size = 0;                                                 \
  }

#endif
