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
/* Takes chunk coordinates within a region. */
enum anvil_err world_load_chunks(struct world *, int x1, int z1, int x2,
				 int z2);
struct chunk *world_chunk_at(struct world *, int x, int z);

void world_free(struct world *w);

#endif
