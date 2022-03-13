#include "world.h"

#include "anvil.h"
#include "mc.h"
#include "nbt.h"
#include "nbt_extra.h"
#include "region.h"
#include "strutil.h"
#include "view.h"

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

	/* TODO: NBT should really just be parsed into structs for stuff like
	 * this. Time for packet-auto-gen part 2, electric boogaloo */
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

enum anvil_err world_load_chunks(struct world *w, int x, int z,
				 int view_distance)
{
	struct view view = { .x = mc_coord_to_chunk(x),
			     .z = mc_coord_to_chunk(z),
			     .size = view_distance };
	int vx, vz;
	VIEW_FOREACH(view, vx, vz)
	{
		int rx = mc_chunk_to_region(vx);
		int rz = mc_chunk_to_region(vz);
		struct region *region = world_region_at(w, rx, rz);
		if (region == NULL) {
			enum anvil_err err =
			    region_open(w->world_path, rx, rz, &region);
			if (err != ANVIL_OK) {
				return err;
			}
			world_add_region(w, region);
		}
		int lcx = mc_localized_chunk(vx);
		int lcz = mc_localized_chunk(vz);
		if (region_get_chunk(region, lcx, lcz) == NULL) {
			struct chunk *chunk = NULL;
			enum anvil_err err = anvil_get_chunk(
			    region, w->block_table, lcx, lcz, &chunk);
			if (err != ANVIL_OK) {
				/* FIXME: should one chunk failing to load
				 *        really cause the whole thing to
				 *        fail? */
				return err;
			}
			region_set_chunk(region, lcx, lcz, chunk);
		}
	}
	return ANVIL_OK;
}

struct chunk *world_chunk_at(struct world *w, int c_x, int c_z)
{
	int r_x = mc_chunk_to_region(c_x);
	int r_z = mc_chunk_to_region(c_z);
	struct region *r = world_region_at(w, r_x, r_z);
	if (r != NULL) {
		return region_get_chunk(r, mc_localized_chunk(c_x),
					mc_localized_chunk(c_z));
	} else {
		return NULL;
	}
}

void world_chunk_dec_players(struct world *w, int c_x, int c_z)
{
	int r_x = mc_chunk_to_region(c_x);
	int r_z = mc_chunk_to_region(c_z);
	struct region *region = world_region_at(w, r_x, r_z);
	if (region != NULL) {
		int lc_x = mc_localized_chunk(c_x);
		int lc_z = mc_localized_chunk(c_z);
		struct chunk *chunk = region_get_chunk(region, lc_x, lc_z);
		if (chunk != NULL) {
			// FIXME: player count is becoming negative???
			--chunk->player_count;
			if (chunk->player_count == 0) {
				region_set_chunk(region, lc_x, lc_z, NULL);
				free_chunk(chunk);
			}
		}
	}
}

void world_free(struct world *w)
{
	free(w->world_path);
	nbt_free(w->level_data);
	hashmap_free(w->block_table, true, free);
	hashmap_free(w->regions, true, (free_item_func) free_region);
}
