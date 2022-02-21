#include "conn.h"
#include "sb_player_position_rotation.h"
#include "world.h"

void protocol_act_sb_player_position_rotation(struct conn *conn,
					      struct world *world, void *data)
{
	(void) world;

	struct sb_player_position_rotation *position = data;
	conn->player->x = position->x;
	conn->player->y = position->feet_y;
	conn->player->z = position->z;
	// FIXME: track rotation and on_ground
}
