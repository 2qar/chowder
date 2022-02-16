#ifndef CHOWDER_WORLD_H
#define CHOWDER_WORLD_H

#include "anvil.h"
#include "hashmap.h"
#include "region.h"

#include <stdint.h>

struct world;

struct world *world_new(char *world_path, struct hashmap *block_table);
/* returns 0 on success, or -1 on error */
int world_load_level_data(struct world *);
uint64_t world_get_spawn(struct world *);
/* Takes region x,z coords */
struct region *world_region_at(struct world *, int x, int z);
/* Takes a global position */
enum anvil_err world_load_chunks(struct world *, int x, int z,
				 int view_distance);
/* Takes global chunk coordinates */
struct chunk *world_chunk_at(struct world *, int c_x, int c_z);

void world_free(struct world *w);

#endif
