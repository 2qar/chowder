#include "region.h"

#include "strutil.h"

#include <assert.h>
#include <stdlib.h>

#define CHUNK_COORD_TO_ARRAY_IDX(n) (n < 0 ? (n + 1) * -1 : n)

enum anvil_err region_open(const char *level_path, int x, int z,
			   struct region **out)
{
	struct region *region = calloc(1, sizeof(struct region));
	if (region == NULL) {
		return ANVIL_NO_MEMORY;
	}
	char *region_file_path = NULL;
	if (asprintf(&region_file_path, "%s/region/r.%d.%d.mca", level_path, x,
		     z)
	    < 0) {
		free(region);
		return ANVIL_NO_MEMORY;
	}
	/* FIXME: the mode here will be an issue when regions eventually
	 *        become mutable */
	region->file = fopen(region_file_path, "r");
	free(region_file_path);
	if (region->file == NULL) {
		free(region);
		return ANVIL_ERRNO;
	}
	region->x = x;
	region->z = z;
	*out = region;
	return ANVIL_OK;
}

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
	fclose(r->file);
	for (int z = 0; z < 32; ++z)
		for (int x = 0; x < 32; ++x)
			if (r->chunks[z][x] != NULL)
				free_chunk(r->chunks[z][x]);
	free(r);
}
