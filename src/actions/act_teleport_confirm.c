#include "teleport_confirm.h"
#include <stdio.h>
#include "conn.h"
#include "world.h"

void protocol_act_teleport_confirm(struct conn *conn, struct world *world, void *data)
{
	(void) world;

	struct teleport_confirm *confirm = data;
	if (confirm->teleport_id != conn->teleport_id) {
		fprintf(stderr, "teleport id mismatch\n\tconfirm = %d\n\treal=%d\n",
				confirm->teleport_id, conn->teleport_id);
	}
}
