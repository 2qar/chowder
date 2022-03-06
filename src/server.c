#include "server.h"

#include "action.h"
#include "config.h"
#include "login.h"
#include "mc.h"
#include "protocol.h"
#include "protocol_autogen.h"
#include "strutil.h"
#include "view.h"
#include "world.h"

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

/* TODO: make a config.h file or smth for these settings */
#define LEVEL_PATH "levels/default"

static struct conn *server_handshake(int sfd, struct packet *p)
{
	struct conn *conn = calloc(1, sizeof(struct conn));
	conn->sfd = sfd;
	conn->packet = p;
	struct handshake handshake_pack = { 0 };
	struct protocol_do_err err;
	PROTOCOL_READ_S(handshake, conn, handshake_pack, err);
	free(handshake_pack.server_address);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		// FIXME: non-shitty errors would be nice
		fprintf(stderr, "server_handshake: failed to read handshake\n");
		free(conn);
		return NULL;
	} else if (handshake_pack.next_state == HANDSHAKE_NEXT_STATE_STATUS) {
		handle_server_list_ping(conn);
		free(conn);
		return NULL;
	} else if (handshake_pack.next_state == HANDSHAKE_NEXT_STATE_LOGIN) {
		return conn;
	} else {
		fprintf(stderr, "server_handshake: invalid state %d\n",
			handshake_pack.next_state);
		free(conn);
		return NULL;
	}
}

static bool is_air(int blockstate)
{
	/* TODO: don't hardcode these */
	return blockstate == 0 || blockstate == 9129 || blockstate == 9130;
}

static void write_chunk_to_packet(struct chunk_data *packet,
				  struct chunk *chunk, int32_t *data_len)
{
	int primary_bit_mask = 0;
	for (int i = 1; i < chunk->sections_len; ++i) {
		int has_blocks = chunk->sections[i]->bits_per_block > 0;
		primary_bit_mask |= (has_blocks << (i - 1));
	}
	packet->primary_bit_mask = primary_bit_mask;
	packet->data_len = 0;
	for (int i = 0; i < chunk->sections_len; ++i) {
		if (chunk->sections[i]->bits_per_block > 0) {
			++(packet->data_len);
		}
	}
	if (packet->data_len > *data_len) {
		*data_len = packet->data_len;
		packet->data =
		    reallocarray(packet->data, packet->data_len,
				 sizeof(struct chunk_data_chunk_section));
		if (packet->data == NULL) {
			perror("reallocarray");
			return;
		}
	}
	packet->biomes = chunk->biomes;
	int j = 0;
	for (int i = 0; i < chunk->sections_len; ++i) {
		struct section *section = chunk->sections[i];
		if (section->bits_per_block > 0) {
			packet->data[j].block_count = 0;
			for (int b = 0; b < TOTAL_BLOCKSTATES; ++b) {
				int palette_idx = read_blockstate_at(
				    section, b % 16, (b / 16) % 16,
				    b / (16 * 16));
				if (!is_air(section->palette[palette_idx])) {
					++(packet->data[j].block_count);
				}
			}
			packet->data[j].bits_per_block =
			    section->bits_per_block;
			packet->data[j].palette_len = section->palette_len;
			packet->data[j].palette = section->palette;
			packet->data[j].data_array_len =
			    BLOCKSTATES_LEN(section->bits_per_block);
			packet->data[j].data_array =
			    (int64_t *) section->blockstates;
			++j;
		}
	}
}

/* FIXME: this should be in a common header */
/* https://wiki.vg/index.php?title=Protocol&oldid=16067#Position */
static void mc_position_to_xyz(uint64_t pos, uint32_t *x, uint16_t *y,
			       uint32_t *z)
{
	*x = pos >> 38;
	*y = pos & 0xFFF;
	*z = (pos >> 12) & 0x3FFFFFF;
}

static int server_initialize_play_state(struct conn *conn, struct world *w)
{
	struct join_game join_packet = { .entity_id = 123, // TODO
					 .gamemode = 1,
					 .dimension =
					     JOIN_GAME_DIMENSION_OVERWORLD,
					 .hashed_seed = 0, // TODO
					 .max_players = 4,
					 .level_type = "default",
					 .view_distance = 10,
					 .reduced_debug_info = false,
					 .enable_respawn_screen = true };
	struct protocol_do_err err =
	    PROTOCOL_WRITE(join_game, conn, &join_packet);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr,
			"server_initialize_play_state(): join_game failed\n");
		return -1;
	}
	puts("joined the game");
	struct client_settings client_settings_pack;
	PROTOCOL_READ_S(client_settings, conn, client_settings_pack, err);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(
		    stderr,
		    "server_initialize_play_state(): client_settings failed\n");
		return -1;
	}
	/* FIXME: this should be the minimum of the client's preference and
	 *        the server's view distance */
	/* FIXME: conn->view_distance should be used pretty much everywhere
	 *        'server_properties.view_distance' is referenced */
	conn->view_distance = server_properties.view_distance;
	free(client_settings_pack.locale);
	struct cb_held_item_change held_item_change_pack = { .slot = 0 };
	err = PROTOCOL_WRITE(cb_held_item_change, conn, &held_item_change_pack);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "server_initialize_play_state(): failed to "
				"send held_item_change failed\n");
		return -1;
	}
	struct window_items window_items_pack = { 0 }; // TODO
	err = PROTOCOL_WRITE(window_items, conn, &window_items_pack);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "server_initialize_play_state(): failed to "
				"send window_items failed\n");
		return -1;
	}

	// TODO: loading spawn should happen on server startup
	uint64_t spawn_location = world_get_spawn(w);
	uint32_t spawn_x = 0;
	uint16_t spawn_y = 0;
	uint32_t spawn_z = 0;
	mc_position_to_xyz(spawn_location, &spawn_x, &spawn_y, &spawn_z);
	if (world_load_chunks(w, spawn_x, spawn_z,
			      server_properties.view_distance)
	    != ANVIL_OK) {
		fprintf(stderr, "failed to load chunks\n");
		return -1;
	}

	struct update_view_position view_pack = { 0 };
	view_pack.chunk_x = mc_coord_to_chunk(spawn_x);
	view_pack.chunk_z = mc_coord_to_chunk(spawn_z);
	err = PROTOCOL_WRITE(update_view_position, conn, &view_pack);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "server_initialize_play_state(): failed to"
				"send 'update view position' packet\n");
		return -1;
	}
	conn->old_chunk_x = view_pack.chunk_x;
	conn->old_chunk_z = view_pack.chunk_z;

	struct chunk_data chunk_data_pack = { 0 };
	chunk_data_pack.full_chunk = true;
	/* TODO: calculate heightmaps / load them from the region file */
	struct nbt *nbt = nbt_new(TAG_Long_Array, "MOTION_BLOCKING");
	struct nbt_array *arr = malloc(sizeof(struct nbt_array));
	int64_t heightmaps[36] = { 0 };
	arr->len = 36;
	arr->data.longs = heightmaps;
	struct nbt *motion_blocking =
	    nbt_get(nbt, TAG_Long_Array, "MOTION_BLOCKING");
	motion_blocking->data.array = arr;
	chunk_data_pack.heightmaps = nbt;
	int32_t data_len = 0;
	int c1_x =
	    mc_coord_to_chunk(spawn_x - server_properties.view_distance * 16);
	int c1_z =
	    mc_coord_to_chunk(spawn_z - server_properties.view_distance * 16);
	int c2_x =
	    mc_coord_to_chunk(spawn_x + server_properties.view_distance * 16);
	int c2_z =
	    mc_coord_to_chunk(spawn_z + server_properties.view_distance * 16);
	struct chunk *chunk = NULL;
	for (int z = c1_z; z <= c2_z; ++z) {
		for (int x = c1_x; x <= c2_x; ++x) {
			chunk = world_chunk_at(w, x, z);
			if (chunk != NULL) {
				++chunk->player_count;
				chunk_data_pack.chunk_x = x;
				chunk_data_pack.chunk_z = z;
				write_chunk_to_packet(&chunk_data_pack, chunk,
						      &data_len);
				err = PROTOCOL_WRITE(chunk_data, conn,
						     &chunk_data_pack);
				if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
					fprintf(stderr,
						"server_initialize_play_state()"
						": failed to send chunk data "
						"for chunk (%d,%d)\n",
						x, z);
				}
			}
		}
	}
	arr->data.longs = NULL;
	nbt_free(nbt);
	free(chunk_data_pack.data);

	struct spawn_position spawn_pos;
	spawn_pos.location = spawn_location;
	err = PROTOCOL_WRITE(spawn_position, conn, &spawn_pos);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "server_initialize_play_state(): failed to "
				"send spawn_position failed\n");
		return -1;
	}

	struct cb_player_position_look pos_and_look = { 0 };
	pos_and_look.x = spawn_x;
	pos_and_look.y = spawn_y;
	pos_and_look.z = spawn_z;
	conn->teleport_id = 123;
	pos_and_look.teleport_id = conn->teleport_id;
	err = PROTOCOL_WRITE(cb_player_position_look, conn, &pos_and_look);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "server_initialize_play_state(): failed to "
				"send player_position_look failed\n");
		return -1;
	}
	conn->player->x = spawn_x;
	conn->player->y = spawn_y;
	conn->player->z = spawn_z;

	struct player_info_player player = { 0 };
	memcpy(player.uuid, conn->player->uuid, 16);
	player.add_player.name = conn->player->username;
	player.add_player.properties_len = 1;
	player.add_player.gamemode = 0;
	player.add_player.ping = 0;
	player.add_player.has_display_name = false;
	struct player_info_property prop;
	prop.name = "textures";
	prop.value = conn->player->textures;
	prop.is_signed = false;
	player.add_player.properties = &prop;
	struct player_info info;
	info.action = PLAYER_INFO_ACTION_ADD_PLAYER;
	info.players_len = 1;
	info.players = &player;
	err = PROTOCOL_WRITE(player_info, conn, &info);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "server_initialize_play_state(): failed to "
				"send player_position_look failed\n");
		return -1;
	}

	puts("sent all of the shit, just waiting on a teleport confirm");

	conn->last_pong = time(NULL);
	return 0;
}

struct conn *server_accept_connection(int sfd, struct packet *p,
				      struct world *w, struct login_ctx *l_ctx)
{
	struct conn *c = server_handshake(sfd, p);
	if (c == NULL) {
		close(sfd);
		return NULL;
	}
	int err = login(c, l_ctx);
	if (err < 0) {
		// TODO: return meaningful errors instead of -1 everywhere
		fprintf(stderr, "error logging in: %d\n", err);
		conn_finish(c);
		return NULL;
	}
	err = server_initialize_play_state(c, w);
	if (err < 0) {
		fprintf(stderr, "error switching to play state: %d\n", err);
		return NULL;
	}
	return c;
}

int server_play(struct conn *conn, struct world *w)
{
	struct pollfd pfd = { .fd = conn->sfd, .events = POLLIN };
	int polled;
	struct protocol_err err = { 0 };
	while ((polled = poll(&pfd, 1, 0)) > 0 && (pfd.revents & POLLIN)) {
		int result = conn_packet_read_header(conn);
		if (result == 0) {
			puts("client closed connection");
			return 0;
		} else if (result < 0) {
			fprintf(stderr, "error parsing packet\n");
			return -1;
		}
		struct protocol_action action =
		    protocol_actions[conn->packet->packet_id];
		if (action.name != NULL) {
			void *data = NULL;
			err = action.read(conn->packet, &data);
			if (err.err_type != PROTOCOL_ERR_SUCCESS) {
				fprintf(stderr,
					"server_play(): error reading %s\n",
					action.name);
			} else {
				action.act(conn, w, data);
				if (action.sends_message) {
					struct message *msg =
					    message_new(conn->player,
							conn->packet->packet_id,
							data, action.free);
					// FIXME: i hate list_append
					list_append(conn->messages_out,
						    sizeof(struct message *),
						    &msg);
				} else {
					action.free(data);
				}
			}
		} else {
			printf("unimplemented packet 0x%02x\n",
			       conn->packet->packet_id);
		}
	}
	if (polled < 0) {
		perror("poll");
		return -1;
	}

	if (time(NULL) - conn->last_pong > 30) {
		puts("client hasn't sent a keep alive in a while, "
		     "disconnecting");
		return 0;
	}

	if (time(NULL) - conn->last_ping > 3) {
		conn->keep_alive_id = rand();
		struct cb_keep_alive keep_alive_pack = {
			.keep_alive_id = conn->keep_alive_id
		};
		struct protocol_do_err do_err =
		    PROTOCOL_WRITE(cb_keep_alive, conn, &keep_alive_pack);
		if (do_err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
			fprintf(stderr,
				"server_play(): error sending keep alive\n");
			return -1;
		}
		conn->last_ping = time(NULL);
	}
	return 1;
}

struct protocol_do_err server_send_messages(struct list *connections,
					    struct list *messages)
{
	struct protocol_do_err err = { 0 };
	while (!list_empty(messages)
	       && err.err_type == PROTOCOL_DO_ERR_SUCCESS) {
		struct list *conns = connections;
		struct message *msg = list_remove(messages);
		struct message_action action = message_actions[msg->packet_id];
		if (action.name != NULL) {
			void *packet = action.message_to_packet(msg);
			while (!list_empty(conns)
			       && err.err_type == PROTOCOL_DO_ERR_SUCCESS) {
				struct conn *conn = list_item(conns);
				err = protocol_do_write(action.write, conn,
							packet);
				conns = list_next(conns);
			}
			action.free(packet);
		} else {
			fprintf(stderr, "no message action for 0x%02x\n",
				msg->packet_id);
		}
		message_free(msg);
	}
	return err;
}

/* FIXME: once again, the errors suck. there needs to be a giant combined error
 *        type or something */
int server_update_view(struct conn *conn, struct world *world)
{
	int new_chunk_x = mc_coord_to_chunk(conn->player->x);
	int new_chunk_z = mc_coord_to_chunk(conn->player->z);
	assert(new_chunk_x != conn->old_chunk_x
	       || new_chunk_z != conn->old_chunk_z);

	struct update_view_position view_pos;
	view_pos.chunk_x = new_chunk_x;
	view_pos.chunk_z = new_chunk_z;
	struct protocol_do_err err =
	    PROTOCOL_WRITE(update_view_position, conn, &view_pos);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr,
			"failed to write update_view_position packet :(\n");
		return -1;
	}

	enum anvil_err load_err = world_load_chunks(
	    world, conn->player->x, conn->player->z, conn->view_distance);
	if (load_err != ANVIL_OK) {
		fprintf(stderr, "failed to load chunks updating view\n");
		return -1;
	}

	struct view old_view = {
		.x = conn->old_chunk_x,
		.z = conn->old_chunk_z,
		.size = conn->view_distance,
	};
	struct view new_view = {
		.x = new_chunk_x,
		.z = new_chunk_z,
		.size = conn->view_distance,
	};

	/* FIXME: the packets written here should probably be put into a queue
	 *        on the connection for writing later so errors don't have to
	 *        be handled here */
	struct chunk_data chunk_data = { 0 };
	chunk_data.full_chunk = true;

	/* FIXME: these heightmaps really need to be handled properly, man */
	struct nbt *nbt = nbt_new(TAG_Long_Array, "MOTION_BLOCKING");
	struct nbt_array *arr = malloc(sizeof(struct nbt_array));
	int64_t heightmaps[36] = { 0 };
	arr->len = 36;
	arr->data.longs = heightmaps;
	struct nbt *motion_blocking =
	    nbt_get(nbt, TAG_Long_Array, "MOTION_BLOCKING");
	motion_blocking->data.array = arr;
	chunk_data.heightmaps = nbt;

	struct chunk *chunk;
	int32_t chunk_data_len = 0;
	int view_x;
	int view_z;
	VIEW_FOREACH(new_view, view_x, view_z)
	{
		if (!VIEW_CONTAINS(old_view, view_x, view_z)) {
			chunk = world_chunk_at(world, view_x, view_z);
			if (chunk != NULL) {
				++chunk->player_count;
				chunk_data.chunk_x = view_x;
				chunk_data.chunk_z = view_z;
				write_chunk_to_packet(&chunk_data, chunk,
						      &chunk_data_len);
				err = PROTOCOL_WRITE(chunk_data, conn,
						     &chunk_data);
				if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
					fprintf(
					    stderr,
					    "failed to write view chunk :(\n");
				}
			} else {
				fprintf(stderr, "null chunky :( (%d,%d)\n",
					view_x, view_z);
			}
		}
	}
	free(chunk_data.data);
	/* FIXME: heightmapsssssss */
	arr->data.longs = NULL;
	nbt_free(nbt);

	struct unload_chunk unload_packet;
	VIEW_FOREACH(old_view, view_x, view_z)
	{
		if (!VIEW_CONTAINS(new_view, view_x, view_z)) {
			world_chunk_dec_players(world, view_x, view_z);
			unload_packet.chunk_x = view_x;
			unload_packet.chunk_z = view_z;
			err =
			    PROTOCOL_WRITE(unload_chunk, conn, &unload_packet);
			if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
				fprintf(stderr,
					"failed to write view unload :(\n");
			}
		}
	}
	conn->requesting_chunks = false;
	return 0;
}
