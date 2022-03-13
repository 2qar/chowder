#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "player_info.h"

int main()
{
	struct player_info player_info = {0};
	player_info.action = PLAYER_INFO_ACTION_ADD_PLAYER;
	player_info.players_len = 1;
	struct player_info_player player = {0};
	player.add_player.name = "tuckerrrrrrrrrr";
	player.add_player.properties_len = 1;
	struct player_info_property prop = {0};
	prop.name = "textures";
	prop.value = "ewogICJ0aW1lc3RhbXAiIDogMTYyMTkxODkwMzI5NiwKICAicHJvZmlsZUlkIiA6ICIyZDAyZDcyZmE3YjI0N2Q4YTNkNGQ3Mzk0YzEyNDUwMCIsCiAgInByb2ZpbGVOYW1lIiA6ICJ0dWNrZXJycnJycnJycnIiLAogICJzaWduYXR1cmVSZXF1aXJlZCIgOiB0cnVlLAogICJ0ZXh0dXJlcyIgOiB7CiAgICAiU0tJTiIgOiB7CiAgICAgICJ1cmwiIDogImh0dHA6Ly90ZXh0dXJlcy5taW5lY3JhZnQubmV0L3RleHR1cmUvMWEyNGEzMzI3NWRlNzk1NDNkNGIzYjIzOTBhNGMwNTNkMGIzMjI3NTE4YTdhMDE1MjI5MTU3MTA4NDQ4NDNmNCIKICAgIH0KICB9Cn0=";
	player.add_player.properties = &prop;
	player_info.players = &player;

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_player_info(t.conn->packet, &player_info);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}
