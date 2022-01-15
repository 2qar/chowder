#include "sb_keep_alive.h"
#include <stdio.h>
#include <time.h>
#include "conn.h"
#include "world.h"

void protocol_act_sb_keep_alive(struct conn *conn, struct world *world, void *data)
{
	(void) world;

	struct sb_keep_alive *keep_alive = data;
	if (keep_alive->keep_alive_id != conn->keep_alive_id) {
		fprintf(stderr, "keep_alive_id mismatch. perish\n"
				"\tgot %ld\n\texpected %ld\n",
				keep_alive->keep_alive_id, conn->keep_alive_id);
	} else {
		conn->last_pong = time(NULL);
	}
}
