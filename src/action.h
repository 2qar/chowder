#ifndef CHOWDER_PROTOCOL_ACTION_H
#define CHOWDER_PROTOCOL_ACTION_H

#include "conn.h"
#include "message.h"
#include "packet.h"
#include "protocol_types.h"
#include "world.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*protocol_act_func)(struct conn *, struct world *, void *data);
typedef void (*protocol_free_func)(void *data);

struct protocol_action {
	const char *name;
	protocol_read_func read;
	protocol_act_func act;
	protocol_free_func free;
	bool sends_message;
};

typedef void *(*message_to_packet_func)(struct message *);

struct message_action {
	const char *name;
	message_to_packet_func message_to_packet;
	protocol_write_func write;
	protocol_free_func free;
};

extern struct protocol_action protocol_actions[UINT8_MAX];
extern struct message_action message_actions[UINT8_MAX];

#endif // CHOWDER_PROTOCOL_ACTION_H
