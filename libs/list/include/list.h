#ifndef CHOWDER_LIST_H
#define CHOWDER_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct list {
	void *data;
	struct list *next;
};

struct list *list_new();

/* creates a new element, assuming the given *data is a pointer to a pointer.
 * prepend and append never allocate space for the list's *data, so it can only
 * hold references to already allocated buffers. no int linked list for you */
void list_prepend(struct list *, size_t data_len, void *data);
void list_append(struct list *, size_t data_len, void *data);

void *list_remove(struct list *);
bool list_empty(struct list *);
void *list_item(struct list *);
struct list *list_next(struct list *);
struct list *list_find(struct list *, bool (*equal)(void *, void *), void *item);
int list_len(struct list *);
void list_free(struct list *);

#endif // CHOWDER_LIST_H
