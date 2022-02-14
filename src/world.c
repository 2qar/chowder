#include "world.h"

#include "anvil.h"
#include "nbt.h"
#include "nbt_extra.h"
#include "region.h"
#include "strutil.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct world {
	char *world_path;
	struct nbt *level_data;
	struct hashmap *block_table;
	struct hashmap *regions;
};

struct world *world_new(char *world_path, struct hashmap *block_table)
{
	struct world *w = malloc(sizeof(struct world));
	w->world_path = world_path;
	w->level_data = NULL;
	w->block_table = block_table;
	w->regions = hashmap_new(1);
	return w;
}

int world_load_level_data(struct world *world)
{
	char *level_data_path;
	if (asprintf(&level_data_path, "%s/level.dat", world->world_path) < 0) {
		return -1;
	}
	FILE *level_data_file = fopen(level_data_path, "r");
	free(level_data_path);
	if (level_data_file == NULL) {
		perror(level_data_path);
		return -1;
	}
	struct nbt *level_data;
	int err = nbt_unpack_file(fileno(level_data_file), &level_data);
	fclose(level_data_file);
	if (err != 0) {
		return -1;
	}
	struct nbt *data = nbt_get(level_data, TAG_Compound, "Data");
	int data_version;
	if (data == NULL) {
		nbt_free(level_data);
		fprintf(stderr, "malformed level.dat\n");
		return -1;
	} else if (!nbt_get_value(data, TAG_Int, "DataVersion", &data_version)
			|| data_version != ANVIL_DATA_VERSION) {
		nbt_free(level_data);
		fprintf(stderr, "incompatible level version\n");
		return -1;
	}

	world->level_data = level_data;
	return 0;
}

/* FIXME: this should be in a public header */
/* https://wiki.vg/index.php?title=Protocol&oldid=16067#Position */
static uint64_t mc_xyz_to_position(uint64_t x, uint16_t y, uint64_t z)
{
	return ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF);
}

uint64_t world_get_spawn(struct world *w)
{
	uint32_t spawn_x = 0;
	uint32_t spawn_y = 0;
	uint32_t spawn_z = 0;

	/* TODO: NBT should really just be parsed into structs for stuff like this.
	 *       Time for packet-auto-gen part 2, electric boogaloo */
	struct nbt *data = nbt_get(w->level_data, TAG_Compound, "Data");
	assert(data != NULL);
	nbt_get_value(data, TAG_Int, "SpawnX", &spawn_x);
	nbt_get_value(data, TAG_Int, "SpawnY", &spawn_y);
	nbt_get_value(data, TAG_Int, "SpawnZ", &spawn_z);
	return mc_xyz_to_position(spawn_x, spawn_y, spawn_z);
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

enum anvil_err world_load_chunks(struct world *w, int x1, int z1, int x2,
				 int z2)
{
	// FIXME: this restriction shouldn't be a thing
	int r_x = x1 / 32;
	int r_z = z1 / 32;
	assert(r_x == x2 / 32 && r_z == z2 / 32);
	size_t region_file_path_len = strlen(w->world_path)
				      + strlen("/region/r...mca") + digits(r_x)
				      + digits(r_z) + 1;
	char *region_file_path = calloc(region_file_path_len, sizeof(char));
	snprintf(region_file_path, region_file_path_len,
		 "%s/region/r.%d.%d.mca", w->world_path, r_x, r_z);
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
		fprintf(stderr, "failed to load chunk at (%d,%d): %d\n",
			ctx.err_x, ctx.err_z, err);
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
	free(w->world_path);
	nbt_free(w->level_data);
	hashmap_free(w->block_table, true, free);
	hashmap_free(w->regions, true, (free_item_func) free_region);
}
