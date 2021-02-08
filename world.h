#include "include/linked_list.h"
#include "region.h"

struct world {
	/* 2D linked-list of loaded regions */
	struct node *regions;
};

struct world *world_new();
void world_add_region(struct world *, struct region *);
/* Takes region x,z coords */
struct region *world_region_at(struct world *, int x, int z);
/* TODO: implement this */
/* Takes world x,z coords */
struct chunk **world_chunks(struct world *, int x1, int x2, int z1, int z2);

void world_remove_region(struct world *, struct region *);
void world_free(struct world *w);
