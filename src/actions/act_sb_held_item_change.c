#include "conn.h"
#include "sb_held_item_change.h"
#include "world.h"

#include <stdio.h>

void protocol_act_sb_held_item_change(struct conn *conn, struct world *world,
				      void *data)
{
	(void) world;

	struct sb_held_item_change *item_change = data;
	// FIXME: Asserts like this shouldn't be here, all it takes is one bad
	//        packet to crash the entire server. Instead, these functions
	//        should return a bool or something indicating whether the
	//        packet was valid or not, and the server can decide whether or
	//        not to disconnect the client based on that.
	assert(item_change->slot >= 0 && item_change->slot <= 8);
	conn->player->selected_slot = item_change->slot;
}
