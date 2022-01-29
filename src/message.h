#ifndef CHOWDER_MESSAGE_H
#define CHOWDER_MESSAGE_H

#include "player.h"
#include "protocol_types.h"

struct message {
	struct player *from;
	int packet_id;
	void *packet_struct;
	void (*packet_free)(void *packet_struct);
};

struct message *message_new(struct player *from, int packet_id,
			    void *packet_struct,
			    void (*packet_free)(void *packet_struct));
void message_free(struct message *);

#endif // CHOWDER_MESSAGE_H
