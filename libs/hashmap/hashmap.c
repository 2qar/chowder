#include "hashmap.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HASHMAP_LOAD_FACTOR 0.8

struct hashmap_entry {
	bool removed;
	char *key;
	void *value;
};

struct hashmap {
	size_t occupied;
	size_t entries_len;
	struct hashmap_entry *entries;
};

struct hashmap *hashmap_new(size_t elems) {
	struct hashmap *hm = malloc(sizeof(struct hashmap));
	hm->occupied = 0;
	hm->entries_len = elems + elems * (1-HASHMAP_LOAD_FACTOR);
	hm->entries = calloc(hm->entries_len, sizeof(struct hashmap_entry));
	return hm;
}

void hashmap_free(struct hashmap *hm, bool free_keys, free_item_func free_item) {
	for (size_t i = 0; i < hm->entries_len; ++i) {
		if (hm->entries[i].key != NULL) {
			if (free_keys) {
				free(hm->entries[i].key);
			}
			free_item(hm->entries[i].value);
		}
	}
	free(hm->entries);
	free(hm);
}

/* https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1a_hash */
static uint64_t fnv1a(char *key) {
	uint64_t hash = 0xcbf29ce484222325;
	while (*key != '\0') {
		hash ^= *key;
		hash *= 0x100000001b3;
		++key;
	}
	return hash;
}

static void hashmap_resize(struct hashmap *hm) {
	size_t old_entries_len = hm->entries_len;
	// FIXME: there's gotta be a better way than allocating another buffer
	struct hashmap_entry *old_entries = hm->entries;
	hm->occupied = 0;
	hm->entries_len = old_entries_len * 2;
	hm->entries = calloc(hm->entries_len, sizeof(struct hashmap_entry));
	for (size_t i = 0; i < old_entries_len; ++i) {
		if (old_entries[i].key != NULL) {
			hashmap_add(hm, old_entries[i].key, old_entries[i].value);
		}
	}
	free(old_entries);
}

static size_t hashmap_index(struct hashmap *hm, char *key) {
	return fnv1a(key) % hm->entries_len;
}

void hashmap_add(struct hashmap *hm, char *key, void *value) {
	if (hm->occupied >= hm->entries_len * HASHMAP_LOAD_FACTOR) {
		hashmap_resize(hm);
	}

	bool looped = false;
	size_t i = hashmap_index(hm, key);
	size_t j = i;
	while (j < hm->entries_len && hm->entries[j].key != NULL) {
		++j;
	}
	if (j == hm->entries_len) {
		looped = true;
		j = 0;
		while (j < i && hm->entries[j].key != NULL) {
			++j;
		}
	}
	assert(!looped || j != i);
	assert(hm->entries[j].key == NULL);
	assert(hm->entries[j].value == NULL);
	hm->entries[j].removed = false;
	hm->entries[j].key = key;
	hm->entries[j].value = value;
	++(hm->occupied);
}

static size_t hashmap_get_index(struct hashmap *hm, char *key) {
	bool looped = false;
	size_t i = hashmap_index(hm, key);
	size_t j = i;
	struct hashmap_entry *e = &(hm->entries[j]);
	while (j < hm->entries_len && (e->removed
				|| (e->key != NULL && strcmp(e->key, key)))) {
		++j;
		e = &(hm->entries[j]);
	}
	if (j == hm->entries_len) {
		looped = true;
		j = 0;
		e = &(hm->entries[j]);
		while (j < i && (e->removed
					|| (e->key != NULL && strcmp(e->key, key)))) {
			++j;
			e = &(hm->entries[j]);
		}
	}
	if ((looped && i == j) || hm->entries[j].key == NULL) {
		return hm->entries_len;
	} else {
		return j;
	}
}

void *hashmap_get(struct hashmap *hm, char *key) {
	size_t i = hashmap_get_index(hm, key);
	if (i == hm->entries_len) {
		return NULL;
	} else {
		return hm->entries[i].value;
	}
}

void *hashmap_remove(struct hashmap *hm, char *key) {
	size_t i = hashmap_get_index(hm, key);
	if (i != hm->entries_len) {
		hm->entries[i].removed = true;
		free(hm->entries[i].key);
		void *value = hm->entries[i].value;
		hm->entries[i].value = NULL;
		--(hm->occupied);
		return value;
	} else {
		return NULL;
	}
}

size_t hashmap_occupied(struct hashmap *hm)
{
	return hm->occupied;
}

void hashmap_apply(struct hashmap *hm, hm_apply_func do_func, void *data)
{
	for (size_t i = 0; i < hm->entries_len; ++i) {
		if (hm->entries[i].key != NULL) {
			do_func(hm->entries[i].key, hm->entries[i].value, data);
		}
	}
}
