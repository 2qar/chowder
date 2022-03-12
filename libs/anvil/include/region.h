#ifndef CHOWDER_REGION_H
#define CHOWDER_REGION_H

#include "anvil_err.h"
#include "chunk.h"
#include "hashmap.h"

#include <stdio.h>

struct region {
	FILE *file;
	int x;
	int z;
	struct chunk *chunks[32][32];
};

enum anvil_err region_open(const char *level_path, int x, int z,
			   struct region **out);

/* set/get assume that chunk_x and chunk_z are actually contained in the given
 * region */
void region_set_chunk(struct region *, int chunk_x, int chunk_z,
		      struct chunk *);
struct chunk *region_get_chunk(struct region *, int chunk_x, int chunk_z);

void free_region(struct region *);

#endif
