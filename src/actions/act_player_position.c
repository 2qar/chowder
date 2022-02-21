#include "conn.h"
#include "mc.h"
#include "player_position.h"
#include "world.h"

void protocol_act_player_position(struct conn *conn, struct world *world,
				  void *data)
{
	(void) world;

	struct player_position *position = data;
	conn->player->x = position->x;
	conn->player->y = position->feet_y;
	conn->player->z = position->z;
	// FIXME: track on_ground
}
