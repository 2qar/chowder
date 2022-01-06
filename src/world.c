#include "world.h"
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "anvil.h"

struct world {
	char *world_path;
	struct hashmap *block_table;
	struct hashmap *regions;
};

struct world *world_new(char *world_path, struct hashmap *block_table)
{
	struct world *w = malloc(sizeof(struct world));
	w->world_path = world_path;
	w->block_table = block_table;
	w->regions = hashmap_new(1);
	return w;
}

static size_t digits(int x)
{
	size_t n = x < 0;
	n += x % 10 == 0;
	while (x != 0) {
		x /= 10;
		++n;
	}
	return n;
}

// FIXME: this is pretty dumb, hashmaps should just work for keys of any type
//        so i don't have to do this hacky stuff but i guess it works for now xd
static char *region_string(int x, int z)
{
	size_t region_str_len = digits(x) + digits(z) + 2;
	char *region_str = calloc(region_str_len, sizeof(char));
	snprintf(region_str, region_str_len, "%d,%d", x, z);
	return region_str;
}

static void world_add_region(struct world *w, struct region *r)
{
	char *region_str = region_string(r->x, r->z);
	hashmap_add(w->regions, region_str, r);
}

struct region *world_region_at(struct world *w, int x, int z)
{
	char *region_str = region_string(x, z);
	struct region *r = hashmap_get(w->regions, region_str);
	free(region_str);
	return r;
}

enum anvil_err world_load_chunks(struct world *w, int x1, int z1, int x2, int z2)
{
	// FIXME: this restriction shouldn't be a thing
	int r_x = x1 / 32;
	int r_z = z1 / 32;
	assert(r_x == x2 / 32 && r_z == z2 / 32);
	size_t region_file_path_len = strlen(w->world_path) + strlen("/region/r...mca")
		+ digits(r_x) + digits(r_z) + 1;
	char *region_file_path = calloc(region_file_path_len, sizeof(char));
	snprintf(region_file_path, region_file_path_len, "%s/region/r.%d.%d.mca",
			w->world_path, r_x, r_z);
	printf("region file path: \"%s\"\n", region_file_path);
	FILE *region_file = fopen(region_file_path, "r");
	free(region_file_path);
	if (region_file == NULL) {
		perror("fopen");
		return -1;
	}
	struct region *r = world_region_at(w, r_x, r_z);
	if (r == NULL) {
		r = calloc(1, sizeof(struct region));
		r->x = r_x;
		r->z = r_z;
		world_add_region(w, r);
	}
	struct anvil_get_chunks_ctx ctx = {
		.region_file = region_file,
		.block_table = w->block_table,
		.x1 = x1,
		.z1 = z1,
		.x2 = x2,
		.z2 = z2,
	};
	enum anvil_err err = anvil_get_chunks(&ctx, r->chunks);
	fclose(region_file);
	if (err != ANVIL_OK) {
		fprintf(stderr, "failed to load chunk at (%d,%d): %d\n", ctx.err_x, ctx.err_z, err);
	}
	return err;
}

struct chunk *world_chunk_at(struct world *w, int x, int z)
{
	struct chunk *c = NULL;

	int r_x = x / 512;
	int r_z = z / 512;
	struct region *r = world_region_at(w, r_x, r_z);
	if (r != NULL) {
		int c_x = (x % 512) / 16;
		int c_z = (z % 512) / 16;
		c = r->chunks[c_z][c_x];
	}

	return c;
}

void world_free(struct world *w)
{
	hashmap_free(w->block_table, true, free);
	hashmap_free(w->regions, true, free);
}
