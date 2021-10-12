/* a hashmap, with strings as keys and pointers to dynamically allocated objects as values */
#include <stdbool.h>
#include <stddef.h>
#ifndef CHOWDER_HASHMAP_H
#define CHOWDER_HASHMAP_H

typedef void (*free_item_func)(void *);

struct bucket_entry {
	char *key;
	void *value;
};
struct hashmap;

struct hashmap *hashmap_new(size_t elems);
void hashmap_free(struct hashmap *, bool free_keys, free_item_func);

/* the hashmap takes ownership of the key, so strings that need to live beyond
 * the hashmap should be strdup()'d */
void hashmap_add(struct hashmap *, char *key, void *value);
void *hashmap_get(struct hashmap *, char *key);
void *hashmap_remove(struct hashmap *, char *key);
size_t hashmap_occupied(struct hashmap *);

typedef void (*hm_apply_func)(char *key, void *value, void *data);
void hashmap_apply(struct hashmap *, hm_apply_func, void *data);

#endif // CHOWDER_HASHMAP_H
