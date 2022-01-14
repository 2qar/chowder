#include "action.h"
#include <stdlib.h>
#include "actions_autogen.h"
#include "protocol_autogen.h"

#define ACTION_CONF(PACKET_NAME, FREE, SENDS_MSG) \
	[PROTOCOL_ID_ ## PACKET_NAME] = { #PACKET_NAME, \
		(protocol_read_func) protocol_read_ ## PACKET_NAME , \
		protocol_act_ ## PACKET_NAME , \
		FREE, \
		SENDS_MSG }
#define ACTION(PACKET_NAME) ACTION_CONF(PACKET_NAME, free, false)
#define ACTIONF(PACKET_NAME) ACTION_CONF(PACKET_NAME, protocol_free_ ## PACKET_NAME, false)
#define ACTIONM(PACKET_NAME) ACTION_CONF(PACKET_NAME, free, true)
#define ACTIONMF(PACKET_NAME) ACTION_CONF(PACKET_NAME, protocol_free_ ## PACKET_NAME, true)

struct protocol_action protocol_actions[UINT8_MAX] = {
	ACTION(teleport_confirm),
	ACTION(sb_keep_alive),
	ACTION(player_block_placement),
	ACTIONMF(sb_chat_message),
};

#define MSG_ACTION(PACKET_NAME, OUT_PACKET_NAME) \
	[PROTOCOL_ID_ ## PACKET_NAME] = { #PACKET_NAME, \
		message_to_packet_ ## PACKET_NAME, \
		(protocol_write_func) protocol_write_ ## OUT_PACKET_NAME, \
		protocol_free_ ## OUT_PACKET_NAME }

struct message_action message_actions[UINT8_MAX] = {
	MSG_ACTION(sb_chat_message, cb_chat_message),
};
