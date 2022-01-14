#include "sb_chat_message.h"
#include <stdio.h>
#include "cb_chat_message.h"
#include "conn.h"
#include "message.h"
#include "strutil.h"
#include "world.h"

void protocol_act_sb_chat_message(struct conn *conn, struct world *world, void *data)
{
	(void) world;

	struct sb_chat_message *chat_message = data;
	printf("<%s> %s\n", conn->player->username, chat_message->message);
}

void *message_to_packet_sb_chat_message(struct message *msg)
{
	struct cb_chat_message *packet = malloc(sizeof(struct cb_chat_message));
	asprintf(&packet->chat_json, "{\"translate\":\"chat.type.text\","
			"\"with\":[{\"text\":\"%s\"},{\"text\":\"%s\"}]}",
			msg->from->username,
			((struct sb_chat_message *) msg->packet_struct)->message);
	packet->position = CB_CHAT_MESSAGE_POSITION_CHAT;
	return packet;
}

// TODO: these should be generated along with the protocol read/write functions
void protocol_free_sb_chat_message(void *data)
{
	struct sb_chat_message *packet = data;
	free(packet->message);
	free(packet);
}

void protocol_free_cb_chat_message(void *data)
{
	struct cb_chat_message *packet = data;
	free(packet->chat_json);
	free(packet);
}
