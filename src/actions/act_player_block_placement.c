#include "chunk.h"
#include "conn.h"
#include "mc.h"
#include "player_block_placement.h"
#include "world.h"

#include <stdint.h>

void protocol_act_player_block_placement(struct conn *conn, struct world *world,
					 void *data)
{
	(void) conn;

	struct player_block_placement *block_placement = data;
	int32_t x, z;
	int16_t y;
	mc_position_to_xyz(block_placement->location, &x, &y, &z);
	switch (block_placement->face) {
	case PLAYER_BLOCK_PLACEMENT_FACE_BOTTOM:
		--y;
		break;
	case PLAYER_BLOCK_PLACEMENT_FACE_TOP:
		++y;
		break;
	case PLAYER_BLOCK_PLACEMENT_FACE_NORTH:
		--z;
		break;
	case PLAYER_BLOCK_PLACEMENT_FACE_SOUTH:
		++z;
		break;
	case PLAYER_BLOCK_PLACEMENT_FACE_WEST:
		--x;
		break;
	case PLAYER_BLOCK_PLACEMENT_FACE_EAST:
		++x;
		break;
	}
	struct chunk *chunk = world_chunk_at(world, x, z);
	int i = (y / 16) + 1;
	if (i < chunk->sections_len && chunk->sections[i]->bits_per_block > 0) {
		printf("INFO: writing blockstate to (%d,%d,%d)\n", x, y, z);
		/* TODO: track what the player is holding and write that block
		 *       instead of some random block from the palette */
		write_blockstate_at(chunk->sections[i], x, y, z,
				    chunk->sections[i]->palette_len - 1);
	}
}
