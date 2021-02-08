#ifndef CHOWDER_LINKED_H
#define CHOWDER_LINKED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct node {
	void *data;
	struct node *next;
};

struct node *list_new();

/* creates a new element, assuming the given *data is a pointer to a pointer.
 * prepend and append never allocate space for the node's *data, so it can only
 * hold references to already allocated buffers. no int linked list for you */
void list_prepend(struct node *list, size_t data_len, void *data);
void list_append(struct node *list, size_t data_len, void *data);

void *list_remove(struct node *list);
bool list_empty(struct node *list);
void *list_item(struct node *list);
struct node *list_next(struct node *list);
struct node *list_find(struct node *list, bool (*equal)(void *, void *), void *item);
int list_len(struct node *list);
void list_free(struct node *list);

#endif
