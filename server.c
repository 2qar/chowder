#include "server.h"
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "login.h"
#include "protocol.h"

/* TODO: make a config.h file or smth for these settings */
#define LEVEL_PATH "levels/default"

struct conn *server_handshake(int sfd, struct packet *p) {
	struct conn *conn = calloc(1, sizeof(struct conn));
	conn->sfd = sfd;
	conn->packet = p;

	int next_state = handshake(conn);
	if (next_state == 1) {
		handle_server_list_ping(conn);
		conn_finish(conn);
		return NULL;
	} else if (next_state == 2) {
		return conn;
	} else {
		fprintf(stderr, "invalid state %d\n", next_state);
		return NULL;
	}
}

int server_initialize_play_state(struct conn *conn, struct world *w, struct hashmap *block_table) {
	join_game(conn);
	puts("joined the game");
	if (client_settings(conn) < 0) {
		fprintf(stderr, "error reading client settings\n");
		return -1;
	}
	if (held_item_change_clientbound(conn, 0) < 0) {
		fprintf(stderr, "error sending held item change\n");
		return -1;
	}
	if (window_items(conn) < 0) {
		fprintf(stderr, "error sending window items\n");
		return -1;
	}

	struct region *r = world_region_at(w, 0, 0);
	if (r == NULL) {
		r = calloc(1, sizeof(struct region));
		world_add_region(w, r);
	}
	FILE *f = fopen(LEVEL_PATH "/region/r.0.0.mca", "r");
	if (f == NULL) {
		fprintf(stderr, "error opening region file\n");
		return -1;
	}
	/* TODO: don't load chunks here or like this pls thanks */
	size_t chunk_buf_len = 0;
	Bytef *chunk_buf = NULL;
	for (int z = 0; z < 16; ++z) {
		for (int x = 0; x < 16; ++x) {
			struct chunk *chunk = r->chunks[z][x];
			if (chunk == NULL) {
				int uncompressed_len = read_chunk(f, x, z, &chunk_buf_len, &chunk_buf);
				if (uncompressed_len > 0) {
					chunk = parse_chunk(block_table, uncompressed_len, chunk_buf);
					if (chunk == NULL) {
						fprintf(stderr, "also panic\n");
						return -1;
					}
				} else if (uncompressed_len < 0) {
					fprintf(stderr, "fuckin panic");
					return -1;
				}
			}

			if (chunk != NULL) {
				chunk_data(conn, chunk, x, z, true);
				r->chunks[z][x] = chunk;
			}
		}
	}
	fclose(f);
	free(chunk_buf);

	if (spawn_position(conn, 0, 0, 0) < 0) {
		fprintf(stderr, "error sending spawn position\n");
		return -1;
	}
	int teleport_id;
	if (player_position_look(conn, &teleport_id) < 0) {
		fprintf(stderr, "error sending position + look\n");
		return -1;
	}

	struct player_info info = {0};
	memcpy(info.uuid, conn->player->uuid, 16);
	memcpy(info.add.username, conn->player->username, sizeof(char) * 16);
	info.add.properties_len = 1;
	struct player_info_property prop;
	prop.name = "textures";
	prop.value = conn->player->textures;
	info.add.properties = &prop;
	if (player_info(conn, PLAYER_INFO_ADD_PLAYER, 1, &info) < 0) {
		fprintf(stderr, "error sending player info\n");
		return -1;
	}

	puts("sent all of the shit, just waiting on a teleport confirm");
	return 0;
}

int server_play(struct conn *conn, struct world *w) {
	/* FIXME: store this info per-connection or things will break */
	uint64_t keep_alive_id;
	time_t keep_alive_time = 0;
	time_t last_client_response = time(NULL);

	struct pollfd pfd = { .fd = conn->sfd, .events = POLLIN };
	int polled = poll(&pfd, 1, 100);
	if (polled > 0 && (pfd.revents & POLLIN)) {
		int result = conn_packet_read_header(conn);
		if (result == 0) {
			puts("client closed connection");
			return 0;
		} else if (result < 0) {
			fprintf(stderr, "error parsing packet\n");
			return -1;
		}
		switch (conn->packet->packet_id) {
			case 0x00:
				printf("teleport confirm: %d\n", teleport_confirm(conn->packet, 123));
				break;
			case 0x0F:
				if (keep_alive_serverbound(conn->packet, keep_alive_id) < 0)
					break;
				last_client_response = time(NULL);
				break;
			case 0x2C:
				player_block_placement(conn->packet, w);
				break;
			default:
				//printf("unimplemented packet 0x%02x\n", p.packet_id);
				break;
		}
	} else if (polled < 0) {
		perror("poll");
		return -1;
	}

	if (time(NULL) - last_client_response >= 30) {
		puts("client hasn't sent a keep alive in a while, disconnecting");
		return 0;
	}

	if (time(NULL) - keep_alive_time > 15) {
		if (keep_alive_clientbound(conn, &keep_alive_time, &keep_alive_id) < 0) {
			fprintf(stderr, "error sending keep alive\n");
			return -1;
		}
	}
	return 1;
}
