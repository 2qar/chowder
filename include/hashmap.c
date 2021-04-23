#include "hashmap.h"

#include <stdbool.h>
#include <stdint.h>
#include "linked_list.h"

struct bucket_entry {
	char *key;
	void *value;
};

struct hashmap {
	size_t buckets_len;
	struct node **buckets;
};

struct hashmap *hashmap_new(size_t elems) {
	struct hashmap *hm = malloc(sizeof(struct hashmap));
	hm->buckets_len = elems + (elems / 4);
	hm->buckets = calloc(hm->buckets_len, sizeof(struct node *));
	return hm;
}

void hashmap_free(struct hashmap *hm, free_item_func free_item) {
	for (size_t i = 0; i < hm->buckets_len; ++i) {
		struct node *bucket = hm->buckets[i];
		while (bucket != NULL && !list_empty(bucket)) {
			struct bucket_entry *b = list_remove(bucket);
			free(b->key);
			free_item(b->value);
			free(b);
		}
		free(bucket);
	}
	free(hm->buckets);
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

static size_t hashmap_index(struct hashmap *hm, char *key) {
	return fnv1a(key) % hm->buckets_len;
}

static struct bucket_entry *make_bucket_entry(char *key, void *value) {
	struct bucket_entry *b = malloc(sizeof(struct bucket_entry));
	b->key = strdup(key);
	b->value = value;
	return b;
}

void hashmap_add(struct hashmap *hm, char *key, void *value) {
	struct bucket_entry *b = make_bucket_entry(key, value);
	size_t i = hashmap_index(hm, key);
	struct node *bucket = hm->buckets[i];
	if (bucket == NULL) {
		bucket = list_new();
		list_append(bucket, sizeof(struct bucket_entry *), &b);
		hm->buckets[i] = bucket;
	} else {
		list_append(bucket, sizeof(struct bucket_entry *), &b);
	}
}

static bool hashmap_bucket_entry_equal(void *p1, void *p2) {
	struct bucket_entry *b1 = p1;
	struct bucket_entry *b2 = p2;
	return strcmp(b1->key, b2->key) == 0;
}

void *hashmap_get(struct hashmap *hm, char *key) {
	size_t i = hashmap_index(hm, key);
	struct node *bucket = hm->buckets[i];
	if (bucket != NULL) {
		struct bucket_entry b = { .key = key };
		struct bucket_entry *v = list_item(list_find(bucket, hashmap_bucket_entry_equal, &b));
		if (v != NULL) {
			return v->value;
		}
	}
	return NULL;
}

void *hashmap_remove(struct hashmap *hm, char *key) {
	size_t i = hashmap_index(hm, key);
	struct node *bucket = hm->buckets[i];
	if (bucket != NULL) {
		struct bucket_entry b = { .key = key };
		struct bucket_entry *v = list_remove(list_find(bucket, hashmap_bucket_entry_equal, &b));
		if (v != NULL) {
			free(v->key);
			void *value = v->value;
			free(v);
			return value;
		}
	}
	return NULL;
}
