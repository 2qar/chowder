#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <zlib.h>

#include "protocol.h"
#include "region.h"
#include "world.h"
#include "nbt.h"

int handshake(struct conn *c) {
	if (conn_packet_read_header(c) < 0)
		// TODO: maybe use more than -1 for error values
		//       so they make sense instead of all being -1
		return -1;

	int protocol_version;
	if (packet_read_varint(c->packet, &protocol_version) < 0)
		return -1;
	
	char ip[1000];
	if (packet_read_string(c->packet, 1000, ip) < 0) {
		return -1;
	}

	uint16_t port;
	if (!packet_read_short(c->packet, &port)) {
		return -1;
	}

	int next_state;
	if (packet_read_varint(c->packet, &next_state) < 0)
		return -1;

	return next_state;
}

int server_list_ping(struct conn *c) {
	make_packet(c->packet, 0x00);

	const int json_len = 1000;
	char *json = malloc(sizeof(char) * json_len);
	/* TODO: don't hardcode, insert state instead (once state exists */
	int real_json_len = snprintf(json, json_len-1, "{ \"version\": { \"name\": \"1.15.2\", \"protocol\": 578 },"
		"\"players\": { \"max\": 4, \"online\": 0, \"sample\": [] },"
		"\"description\": { \"text\": \"description\" } }");
	if (real_json_len >= json_len - 1) {
		fprintf(stderr, "attempted to write %d bytes to JSON buffer, consider increasing length\n", real_json_len);
		return -1;
	}
	int n = packet_write_string(c->packet, json_len - 1, json);
	free(json);
	if (n < 0) {
		return n;
	}

	return conn_write_packet(c);
}

int login_start(struct conn *c, char username[]) {
	if (conn_packet_read_header(c) < 0)
		return -1;
	int len = packet_read_string(c->packet, 17, username);
	if (len < 0) {
		return -1;
	} else if (len > 16) {
		fprintf(stderr, "login_start: expected username to be 16 characters, got %d\n", len);
		return -1;
	}
	return 0;
}

int encryption_request(struct conn *c, size_t der_len, const unsigned char *der, uint8_t verify[4]) {
	make_packet(c->packet, 0x01);

	char server_id[20];
	memset(server_id, ' ', 20);
	int n = packet_write_string(c->packet, 20, server_id);
	if (n < 0) {
		return n;
	}

	n = packet_write_varint(c->packet, der_len);
	if (n < 0) {
		return n;
	}
	n = packet_write_bytes(c->packet, der_len, der);
	if (n < 0) {
		return n;
	}

	/* verify token */
	n = packet_write_varint(c->packet, 4);
	if (n < 0) {
		return n;
	}
	int i = 0;
	while (i < 4 && n > 0) {
		packet_write_byte(c->packet, (verify[i++] = (rand() % 255)));
	}
	if (n < 0) {
		return n;
	}

	/* TODO: maybe make the caller call conn_write_packet() and handle the
	 *       errors there so it doesn't get mixed in w/ all the other errors
	 *       these functions can return */
	return conn_write_packet(c);
}

int decrypt_byte_array(struct packet *p, EVP_PKEY_CTX *ctx, size_t len, uint8_t *out) {
	int bytes;
	if (packet_read_varint(p, &bytes) < 0 || bytes != 128) {
		return -1;
	}
	uint8_t encrypted[128];
	int i = 0;
	while (i < 128 && packet_read_byte(p, &(encrypted[i])))
		++i;
	if (i != 128)
		return -1;

	int n;
	if ((n = EVP_PKEY_decrypt(ctx, out, &len, encrypted, 128)) < 0) {
		fprintf(stderr, "EVP_PKEY_decrypt: %lu", ERR_get_error());
		return -1;
	}
	return n;
}

int encryption_response(struct conn *c, EVP_PKEY_CTX *ctx, const uint8_t verify[4], uint8_t secret[16]) {
	if (conn_packet_read_header(c) < 0)
		return -1;

	const size_t buf_len = 1000;
	uint8_t *buf = malloc(sizeof(uint8_t) * buf_len);
	if (decrypt_byte_array(c->packet, ctx, buf_len, buf) < 0) {
		return -1;
	}
	for (int i = 0; i < 16; ++i) {
		secret[i] = buf[i];
	}

	/* read client verify */
	if (decrypt_byte_array(c->packet, ctx, buf_len, buf) < 0) {
		return -1;
	}
	/* verify the verifys */
	for (int i = 0; i < 4; ++i) {
		if (buf[i] != verify[i]) {
			fprintf(stderr, "verify mismatch!\nclient: %d-%d-%d-%d\nserver: %d-%d-%d-%d",
					buf[0], buf[1], buf[2], buf[3],
					verify[0], verify[1], verify[2], verify[3]);
			return -1;
		}
	}

	free(buf);
	return 0;
}

int login_success(struct conn *c, const char uuid[36], const char username[16]) {
	make_packet(c->packet, 0x02);
	int n = packet_write_string(c->packet, 36, uuid);
	if (n < 0) {
		return n;
	}
	n = packet_write_string(c->packet, 16, username);
	if (n < 0) {
		return n;
	}
	return conn_write_packet(c);
}

int ping(struct conn *c, uint8_t l[8]) {
	if (conn_packet_read_header(c) < 0)
		return -1;

	int i = 0;
	while (i < 8 && packet_read_byte(c->packet, &(l[i])))
		++i;
	if (i != 8)
		return -1;

	return 0;
}

int pong(struct conn *c, uint8_t l[8]) {
	make_packet(c->packet, 0x01);

	int n = packet_write_bytes(c->packet, 8, l);
	if (n < 0) {
		return n;
	}
	return conn_write_packet(c);
}

int join_game(struct conn *c) {
	struct packet *p = c->packet;
	make_packet(p, 0x26);

	/* TODO: keep track of EID for each player */
	int n = packet_write_int(p, 123);
	if (n < 0) {
		return n;
	}
	/* gamemode */
	n = packet_write_byte(p, 1);
	if (n < 0) {
		return n;
	}
	/* dimension */
	n = packet_write_int(p, 0);
	if (n < 0) {
		return n;
	}

	/* TODO: pass a valid SHA-256 hash */
	n = packet_write_long(p, 0);
	if (n < 0) {
		return n;
	}

	/* max players, ignored */
	n = packet_write_byte(p, 0);
	if (n < 0) {
		return n;
	}

	char level_type[16] = "default";
	n = packet_write_string(p, 16, level_type);
	if (n < 0) {
		return n;
	}

	/* view distance */
	n = packet_write_varint(p, 10);
	if (n < 0) {
		return n;
	}

	/* reduced debug info */
	n = packet_write_byte(p, false);
	if (n < 0) {
		return n;
	}

	/* enable respawn screen */
	n = packet_write_byte(p, true);
	if (n < 0) {
		return n;
	}

	return conn_write_packet(c);
}

int client_settings(struct conn *c) {
	if (conn_packet_read_header(c) < 0)
		return -1;
	struct packet *p = c->packet;

	char locale[17] = {0};
	if (packet_read_string(p, 17, locale) < 0)
		return -1;
	puts(locale);

	uint8_t view_distance;
	if (!packet_read_byte(p, &view_distance))
		return -1;
	printf("view distance: %d\n", view_distance);

	int chat_mode;
	if (packet_read_varint(p, &chat_mode) < 0)
		return -1;
	printf("chat mode: %d\n", chat_mode);

	uint8_t chat_colors;
	if (!packet_read_byte(p, &chat_colors))
		return -1;
	printf("chat colors: %d\n", chat_colors);

	uint8_t displayed_skin;
	if (!packet_read_byte(p, &displayed_skin))
		return -1;
	printf("displayed skin shit: %d\n", displayed_skin);

	int main_hand;
	if (packet_read_varint(p, &main_hand) < 0)
		return -1;
	printf("main hand: %d\n", main_hand);

	return 0;

}

int window_items(struct conn *c) {
	make_packet(c->packet, 0x15);

	/* window ID */
	int n = packet_write_byte(c->packet, 0);
	if (n < 0) {
		return n;
	}
	/* slot count */
	n = packet_write_short(c->packet, 0);
	if (n < 0) {
		return n;
	}

	/* TODO: slot array https://wiki.vg/Slot_Data */

	return conn_write_packet(c);
}

int held_item_change_clientbound(struct conn *c, uint8_t slot) {
	make_packet(c->packet, 0x40);

	int n = packet_write_byte(c->packet, slot);
	if (n < 0) {
		return n;
	}
	return conn_write_packet(c);
}

int spawn_position(struct conn *c, uint16_t x, uint16_t y, uint16_t z) {
	make_packet(c->packet, 0x4E);

	/* https://wiki.vg/Protocol#Position */
	uint64_t pos = (((uint64_t)x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF);
	int n = packet_write_long(c->packet, pos);
	if (n < 0) {
		return n;
	}
	return conn_write_packet(c);
}

bool is_air(int blockstate) {
	/* TODO: don't hardcode these */
	return blockstate == 0 || blockstate == 9129 || blockstate == 9130;
}

int write_section_to_packet(const struct section *s, struct packet *p, size_t *data_len) {
	if (s->bits_per_block == -1) {
		*data_len = 0;
		return 0;
	}

	/* count non-air blocks */
	uint16_t block_count = 0;
	for (int i = 0; i < TOTAL_BLOCKSTATES; ++i) {
		int palette_idx = read_blockstate_at(s, i % 16, (i / 16) % 16, i / (16*16));
		if (!is_air(s->palette[palette_idx])) {
			++block_count;
		}
	}
	int n = packet_write_short(p, block_count);
	if (n < 0) {
		return n;
	}

	/* palette */
	const uint8_t bits_per_block = s->bits_per_block;
	n = packet_write_byte(p, bits_per_block);
	if (n < 0) {
		return n;
	}
	uint8_t palette_len = s->palette_len;
	n = packet_write_varint(p, palette_len);
	if (n < 0) {
		return n;
	}
	for (int i = 0; i < palette_len; ++i) {
		n = packet_write_varint(p, s->palette[i]);
		if (n < 0) {
			return n;
		}
	}

	/* write the blocks */
	size_t blockstates_len = BLOCKSTATES_LEN(s->bits_per_block);
	n = packet_write_varint(p, blockstates_len);
	if (n < 0) {
		return n;
	}
	for (size_t i = 0; i < blockstates_len; ++i) {
		n = packet_write_long(p, s->blockstates[i]);
		if (n < 0) {
			return n;
		}
	}

	*data_len = p->packet_len;
	return 0;
}

int chunk_data(struct conn *c, const struct chunk *chunk, int x, int y, bool full) {
	struct packet *p = c->packet;
	make_packet(p, 0x22);

	int n = packet_write_int(p, x);
	if (n < 0) {
		return n;
	}
	n = packet_write_int(p, y);
	if (n < 0) {
		return n;
	}
	n = packet_write_byte(p, full);
	if (n < 0) {
		return n;
	}

	/* primary bit mask */
	int section_bit_mask = 0;
	for (int i = 1; i < chunk->sections_len; ++i) {
		int has_blocks = chunk->sections[i]->bits_per_block > 0;
		section_bit_mask |= (has_blocks << (i - 1));
	}
	n = packet_write_varint(p, section_bit_mask);
	if (n < 0) {
		return n;
	}

	/* TODO: actually calculate heightmaps */
	struct nbt *nbt = nbt_new(TAG_Long_Array, "MOTION_BLOCKING");
	struct nbt_array *arr = malloc(sizeof(struct nbt_array));
	int64_t heightmaps[36] = {0};
	arr->len = 36;
	arr->data.longs = heightmaps;
	nbt->data.array = arr;
	n = packet_write_nbt(p, nbt);
	arr->data.longs = NULL;
	nbt_free(nbt);
	if (n < 0) {
		return n;
	}

	if (full && chunk->biomes != NULL) {
		for (int i = 0; i < BIOMES_LEN; ++i) {
			n = packet_write_int(p, chunk->biomes[i]);
			if (n < 0) {
				return n;
			}
		}
	} else if (full) {
		for (int i = 0; i < BIOMES_LEN; ++i) {
			/* void biome */
			n = packet_write_int(p, 127);
			if (n < 0) {
				return n;
			}
		}
	}

	/* Write each chunk in this[0] format to temporary buffers
	 * and record the total length of each buffer for writing
	 * to the main packet.
	 *    [0]: https://wiki.vg/Chunk_Format#Chunk_Section_structure
	 */
	size_t data_len = 0;
	/* TODO: just use one packet here instead of an array of them */
	struct packet *block_data = calloc(chunk->sections_len, sizeof(struct packet));
	for (int i = 1; i < chunk->sections_len; ++i) {
		packet_init(&(block_data[i]));
		size_t section_len;
		n = write_section_to_packet(chunk->sections[i], &(block_data[i]), &section_len);
		if (n < 0) {
			return n;
		}
		data_len += section_len;
	}

	/* write sections from before */
	n = packet_write_varint(p, data_len);
	if (n < 0) {
		return n;
	}
	for (int s = 0; s < chunk->sections_len; ++s) {
		for (int i = 0; i < block_data[s].packet_len; ++i) {
			n = packet_write_byte(p, block_data[s].data[i]);
			if (n < 0) {
				return n;
			}
		}
		free(block_data[s].data);
	}
	free(block_data);

	/* # of block entities */
	/* TODO: implement block entities */
	n = packet_write_varint(p, 0);
	if (n < 0) {
		return n;
	}

	return conn_write_packet(c);
}

int player_position_look(struct conn *c, int *server_teleport_id) {
	struct packet *p = c->packet;
	make_packet(p, 0x36);

	double x = 0;
	double y = 0;
	double z = 0;
	int n = packet_write_double(p, x);
	if (n < 0) {
		return n;
	}
	n = packet_write_double(p, y);
	if (n < 0) {
		return n;
	}
	n = packet_write_double(p, z);
	if (n < 0) {
		return n;
	}

	float yaw = 0;
	float pitch = 0;
	n = packet_write_float(p, yaw);
	if (n < 0) {
		return n;
	}
	n = packet_write_float(p, pitch);
	if (n < 0) {
		return n;
	}

	uint8_t flags = 0;
	n = packet_write_byte(p, flags);
	if (n < 0) {
		return n;
	}

	/* TODO: randomize it, probably */
	*server_teleport_id = 123;
	n = packet_write_varint(p, *server_teleport_id);
	if (n < 0) {
		return n;
	}

	return conn_write_packet(c);
}

int teleport_confirm(struct packet *p, int server_teleport_id) {
	int teleport_id;
	if (packet_read_varint(p, &teleport_id) < 0) {
		fprintf(stderr, "reading teleport id failed\n");
		return -1;
	} else if (teleport_id != server_teleport_id) {
		fprintf(stderr, "teleport ID mismatch: %d != %d\n", teleport_id, server_teleport_id);
		return -1;
	}

	return 0;
}

int keep_alive_clientbound(struct conn *c, time_t *t, uint64_t *id) {
	make_packet(c->packet, 0x21);

	*id = rand();
	int n = packet_write_long(c->packet, *id);
	if (n < 0) {
		return n;
	}
	*t = time(NULL);

	return conn_write_packet(c);
}

int keep_alive_serverbound(struct packet *p, uint64_t id) {
	uint64_t client_id;
	if (!packet_read_long(p, &client_id)) {
		return -1;
	} else if (client_id != id) {
		/* FIXME: maybe this function isn't the right place for handling the ID mismatch */
		fprintf(stderr, "keep alive ID mismatch\n");
		return -1;
	}
	return 0;
}

int player_block_placement(struct packet *p, struct world *w) {
	int hand;
	if (packet_read_varint(p, &hand) < 0)
		return -1;
	else if (hand < 0 || hand > 1)
		return -1;

	int32_t x, z;
	int16_t y;
	if (!packet_read_position(p, &x, &y, &z))
		return -1;

	int face;
	if (packet_read_varint(p, &face) < 0)
		return -1;

	switch (face) {
		case 0:
			--y;
			break;
		case 1:
			++y;
			break;
		case 2:
			--z;
			break;
		case 3:
			++z;
			break;
		case 4:
			--x;
			break;
		case 5:
			++x;
			break;
		default:
			return -1;
	}
	printf("INFO: read pos (%d,%d,%d)\n", x, y, z);

	/* TODO: implement read_float() for cursor pos x,y,z */
	p->index += 12;

	uint8_t head_in_block;
	if (!packet_read_byte(p, &head_in_block))
		return -1;

	struct chunk *c = world_chunk_at(w, x, z);
	int i = (y / 16) + 1;
	if (i < c->sections_len && c->sections[i]->bits_per_block > 0) {
		printf("INFO: writing blockstate to (%d,%d,%d)\n", x, y, z);
		/* TODO: track what the player is holding and write that block
		 *       instead of some random block from the palette */
		write_blockstate_at(c->sections[i], x, y, z, c->sections[i]->palette_len - 1);
	}

	return 0;
}
