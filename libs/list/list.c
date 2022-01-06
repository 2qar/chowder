#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct list *list_new() {
	return calloc(1, sizeof(struct list));
}

void list_prepend(struct list *list, size_t data_len, void *data) {
	struct list *tmp = malloc(sizeof(struct list));
	memcpy(tmp, list, sizeof(struct list));

	memcpy(&(list->data), data, data_len);
	list->next = tmp;
}

void list_append(struct list *list, size_t data_len, void *data) {
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
void *list_remove(struct list *list) {
	void *data = list->data;
	struct list *next = list->next;
	*list = *(list->next);
	free(next);
	return data;
}

bool list_empty(struct list *list) {
	return list->data == NULL && list->next == NULL;
}

void *list_item(struct list *list) {
	void *item = NULL;
	if (!list_empty(list)) {
		item = list->data;
	}
	return item;
}

struct list *list_next(struct list *list) {
	struct list *next = NULL;
	if (list != NULL) {
		next = list->next;
	}
	return next;
}

struct list *list_find(struct list *list, bool (*equal)(void *, void *), void *item) {
	while (!list_empty(list) && !((*equal)(list_item(list), item))) {
		list = list_next(list);
	}

	return list;
}

int list_len(struct list *list) {
	if (list_empty(list)) {
		return 0;
	} else {
		return 1 + list_len(list->next);
	}
}

void list_free(struct list *list) {
	while (!list_empty(list)) {
		free(list_remove(list));
	}
	free(list);
}
