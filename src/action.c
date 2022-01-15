#include "action.h"
#include <stdlib.h>
#include "actions_autogen.h"
#include "protocol_autogen.h"

#define ACTION_WITH_FREE(PACKET_NAME, FREE) \
	[PROTOCOL_ID_ ## PACKET_NAME] = { #PACKET_NAME, \
		(protocol_read_func) protocol_read_ ## PACKET_NAME , \
		protocol_act_ ## PACKET_NAME , \
		FREE }
#define ACTION(PACKET_NAME) ACTION_WITH_FREE(PACKET_NAME, free)
#define ACTIONF(PACKET_NAME) ACTION_WITH_FREE(PACKET_NAME, protocol_free_ ## PACKET_NAME)

struct protocol_read_action protocol_read_actions[UINT8_MAX] = {
	ACTION(teleport_confirm),
	ACTION(sb_keep_alive),
	ACTION(player_block_placement),
};
