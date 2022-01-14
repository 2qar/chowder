#include "message.h"
#include <stdlib.h>

struct message *message_new(struct player *from, int packet_id, void *packet_struct,
		void (*packet_free)(void *))
{
	struct message *message = malloc(sizeof(struct message));
	message->from = from;
	message->packet_id = packet_id;
	message->packet_struct = packet_struct;
	message->packet_free = packet_free;
	return message;
}

void message_free(struct message *message)
{
	message->packet_free(message->packet_struct);
	free(message);
}
