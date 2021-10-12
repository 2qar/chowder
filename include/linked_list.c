#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct node *list_new() {
	return calloc(1, sizeof(struct node));
}

void list_prepend(struct node *list, size_t data_len, void *data) {
	struct node *tmp = malloc(sizeof(struct node));
	memcpy(tmp, list, sizeof(struct node));

	// FIXME: i see no reason why this can't just be
	//           list->data = data;
	//        when list_prepend() and list_append() are documented
	//        to only allow heap-allocated objects as the data param
	memcpy(&(list->data), data, data_len);
	list->next = tmp;
}

void list_append(struct node *list, size_t data_len, void *data) {
	if (list_empty(list)) {
		list_prepend(list, data_len, data);
	} else {
		while (!list_empty(list->next)) {
			list = list_next(list);
		}

		list_prepend(list->next, data_len, data);
	}
}

/* returns the removed node's data */
void *list_remove(struct node *list) {
	void *data = list->data;
	struct node *next = list->next;
	*list = *(list->next);
	free(next);
	return data;
}

bool list_empty(struct node *list) {
	return list->data == NULL && list->next == NULL;
}

void *list_item(struct node *list) {
	void *item = NULL;
	if (!list_empty(list)) {
		item = list->data;
	}
	return item;
}

struct node *list_next(struct node *list) {
	struct node *next = NULL;
	if (list != NULL) {
		next = list->next;
	}
	return next;
}

struct node *list_find(struct node *list, bool (*equal)(void *, void *), void *item) {
	while (!list_empty(list) && !((*equal)(list_item(list), item))) {
		list = list_next(list);
	}

	return list;
}

int list_len(struct node *list) {
	if (list_empty(list)) {
		return 0;
	} else {
		return 1 + list_len(list->next);
	}
}

void list_free(struct node *list) {
	while (!list_empty(list)) {
		free(list_remove(list));
	}
	free(list);
}

void list_free_nodes(struct node *list) {
	while (!list_empty(list)) {
		struct node *next = list_next(list);
		free(list);
		list = next;
	}
	free(list);
}
