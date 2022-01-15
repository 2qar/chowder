#ifndef CHOWDER_PROTOCOL_ACTION_H
#define CHOWDER_PROTOCOL_ACTION_H

#include <stdint.h>
#include "conn.h"
#include "packet.h"
#include "protocol_types.h"
#include "world.h"

typedef struct protocol_err (*protocol_read_func)(struct packet *, void **data);
typedef void (*protocol_act_func)(struct conn *, struct world *, void *data);
typedef void (*protocol_free_func)(void *data);

struct protocol_read_action {
	const char *name;
	protocol_read_func read;
	protocol_act_func act;
	protocol_free_func free;
};

extern struct protocol_read_action protocol_read_actions[UINT8_MAX];

#endif // CHOWDER_PROTOCOL_ACTION_H
