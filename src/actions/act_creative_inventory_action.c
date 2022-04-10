#include "creative_inventory_action.h"
#include "conn.h"
#include "world.h"

#include <stdint.h>

void protocol_act_creative_inventory_action(struct conn *conn, struct world *world,
					 void *data)
{
	(void) conn;
	(void) world;

	struct creative_inventory_action *action = data;
	// FIXME: check other stuff, like making sure blocks aren't being
	//        placed in armor slots
	assert((action->slot >= 5 && action->slot <= 44) || action->slot == -1);
	if (conn->player->inventory[action->slot].item_id != 0) {
		nbt_free(conn->player->inventory[action->slot].nbt);
	}
	if (action->slot != -1) {
		memcpy(&conn->player->inventory[action->slot], action->clicked_item, sizeof(struct slot));
		action->clicked_item->nbt = NULL;
	}
}

void protocol_free_creative_inventory_action(void *data)
{
	struct creative_inventory_action *packet = data;
	slot_free(packet->clicked_item);
	free(packet);
}
