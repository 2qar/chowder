#include "world.h"
#include "region.h"
#include <stdlib.h>
#include <stdbool.h>

struct world *world_new() {
	struct world *w = malloc(sizeof(struct world));
	w->regions = list_new();
	return w;
}

bool region_list_z_equal(void *list, void *item) {
	struct region *r1 = item;
	struct region *r2 = list_item((struct node *) list);
	return r1->z == r2->z;
}

bool region_list_z_greater(void *list, void *item) {
	struct region *r1 = item;
	struct region *r2 = list_item((struct node *) list);
	return r2->z > r1->z;
}

bool region_list_x_greater(void *list, void *item) {
	struct region *r1 = item;
	struct region *r2 = list_item(list);
	return r2->x > r1->x;
}

void world_add_region(struct world *w, struct region *r) {
	struct node *region_list = list_find(w->regions, &region_list_z_equal, r);

	if (list_empty(region_list)) {
		struct node *l = list_new();
		list_prepend(l, sizeof(struct region *), &r);

		struct node *greater_z = list_find(w->regions, &region_list_z_greater, r);
		list_prepend(greater_z, sizeof(struct node *), &l);
	} else {
		region_list = list_find(region_list, &region_list_x_greater, r);
		list_prepend(region_list, sizeof(struct region *), r);
	}
}

bool region_list_x_equal(void *list, void *item) {
	struct region *r1 = item;
	struct region *r2 = list;
	return r1->x == r2->x;
}

struct node *find_region_sublist(struct world *w, struct region *r) {
	struct node *sublist = NULL;

	struct node *n = list_find(w->regions, &region_list_z_equal, r);
	if (!list_empty(n)) {
		n = list_find(list_item(n), &region_list_x_equal, r);
		if (!list_empty(n)) {
			sublist = n;
		}
	}

	return sublist;
}

struct region *world_region_at(struct world *w, int x, int z) {
	struct region search_region = {0};
	search_region.x = x;
	search_region.z = z;

	struct node *list = find_region_sublist(w, &search_region);
	if (list == NULL) {
		return NULL;
	} else {
		return list_item(list);
	}
}

void world_remove_region(struct world *w, struct region *r) {
	struct node *list = find_region_sublist(w, r);
	if (list != NULL) {
		list_remove(list);
	}
}

void world_free(struct world *w) {
	struct node *regions_list = w->regions;
	while (!list_empty(regions_list)) {
		struct node *region_list = list_item(regions_list);
		while (!list_empty(region_list)) {
			free(list_remove(region_list));
		}
		regions_list = list_next(regions_list);
	}
}
