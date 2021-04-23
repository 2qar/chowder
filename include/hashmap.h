/* a hashmap, with strings as keys and pointers to dynamically allocated objects as values */
#include <stddef.h>

typedef void (*free_item_func)(void *);

struct hashmap;

struct hashmap *hashmap_new(size_t elems);
void hashmap_free(struct hashmap *, free_item_func);

void hashmap_add(struct hashmap *, char *key, void *value);
void *hashmap_get(struct hashmap *, char *key);
void *hashmap_remove(struct hashmap *, char *key);
