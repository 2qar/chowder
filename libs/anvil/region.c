#include "region.h"

#include <assert.h>
#include <stdlib.h>

#define CHUNK_COORD_TO_ARRAY_IDX(n) (n < 0 ? (n + 1) * -1 : n)

void region_set_chunk(struct region *r, int c_x, int c_z, struct chunk *chunk)
{
	c_x = CHUNK_COORD_TO_ARRAY_IDX(c_x);
	c_z = CHUNK_COORD_TO_ARRAY_IDX(c_z);

	assert(c_x < 32 && c_z < 32);
	r->chunks[c_z][c_x] = chunk;
}

struct chunk *region_get_chunk(struct region *r, int c_x, int c_z)
{
	c_x = CHUNK_COORD_TO_ARRAY_IDX(c_x);
	c_z = CHUNK_COORD_TO_ARRAY_IDX(c_z);

	assert(c_x < 32 && c_z < 32);
	return r->chunks[c_z][c_x];
}

void free_region(struct region *r)
{
	for (int z = 0; z < 32; ++z)
		for (int x = 0; x < 32; ++x)
			if (r->chunks[z][x] != NULL)
				free_chunk(r->chunks[z][x]);
	free(r);
}
