
// Simple linked list version of map
// In future this should probably turn into a hashtable or trie,
// but this implementation is easier and less prone to cause bugs for now.

#ifdef CS_DEBUG_MAPS
#define DBG_DUMP(typename, map) map_dump_##typename(map)
#else
#define DBG_DUMP(typename, map)
#endif

#define MAP_DECLARE(key_t, value_t, typename) struct typename { key_t key; value_t value; struct typename * next; }; \
  value_t map_deref_##typename(key_t key, struct typename** map); \
  void map_dump_##typename(struct typename* map); \
  void map_destroy_##typename(struct typename* map);

#define MAP_DECLARE_FUNCTIONS(key_t, value_t, typename) \
  value_t map_deref_##typename(key_t key, struct typename** map) { \
  if(!key) \
    return NULL; \
  while(*map) { \
  if((*map)->key == key)			\
    return (*map)->value;			\
  map = &((*map)->next);			\
  } \
  return NULL;\
  }\
  \
  void map_destroy_##typename(struct typename* map) { \
    if(map) {					      \
      map_destroy_##typename(map->next); \
      free(map); \
    } \
  }
  

#define MAP_DECLARE_DUMP(key_fmt, value_fmt, typename) \
  void map_dump_##typename(struct typename* map) { \
  printf("Dumping map of type " #typename ":\n"); \
  while(map) { \
    printf(#key_fmt " --> " #value_fmt "\n", map->key, map->value);	\
  map = map->next; \
  } \
  printf("Done\n"); \
  }


#define MAP_INIT(key_t, value_t, ppmap) *ppmap = 0;

#define MAP_ADD(key_t, value_t, typename, ppmap, keyv, valuev) {	\
  struct typename* new_entry = CALLOC_STRUCT(typename); \
  new_entry->key = keyv; \
  new_entry->value = valuev; \
  struct typename** next_entry = ppmap; \
  while(*next_entry) { \
    next_entry = &((*next_entry)->next);	\
  } \
  *next_entry = new_entry ; \
  DBG_DUMP(typename, (*ppmap));			\
  }

#define MAP_DEREF(key_t, value_t, typename, ppmap, keyv) ( map_deref_##typename(keyv, ppmap))

#define MAP_DELETE(key_t, value_t, typename, ppmap, keyv) { \
  struct typename** next = ppmap; \
  while(*next) { \
    if((*next)->key == keyv) {			\
      struct typename* tofree = *next;		\
      *next = (*next)->next; \
      free(tofree); \
      break; \
    } \
    next = &((*next)->next);			\
  } \
  DBG_DUMP(typename, (*ppmap));			\
  }

#define MAP_FOR_EACH_VALUE(value_t, map, map_t, func, args...) { \
  struct map_t* next = map; \
  while(next) {		     \
  func(next->value, ## args); \
  next = next->next; \
  } \
  }

#define MAP_DESTROY(map_t, map) map_destroy_##map_t(map); map = NULL;
