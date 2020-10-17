#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <zlib.h>

#include "protocol.h"
#include "region.h"
#include "nbt.h"

int handshake(int sfd) {
	struct recv_packet *p = malloc(sizeof(struct recv_packet));
	if (parse_packet(p, sfd) < 0)
		// TODO: maybe use more than -1 for error values
		//       so they make sense instead of all being -1
		return -1;

	int protocol_version;
	if (read_varint(p, &protocol_version) < 0)
		return -1;
	
	char ip[1000];
	if (read_string(p, ip) < 0) {
		return -1;
	}

	uint16_t port;
	read_ushort(p, &port);

	int next_state;
	if (read_varint(p, &next_state) < 0)
		return -1;

	free(p);
	return next_state;
}

int server_list_ping(int sfd) {
	struct send_packet p = {0};
	make_packet(&p, 0x00);

	const int json_len = 1000;
	char *json = malloc(sizeof(char) * json_len);
	/* TODO: don't hardcode, insert state instead (once state exists */
	int n = snprintf(json, json_len-1, "{ \"version\": { \"name\": \"1.15.2\", \"protocol\": 578 },"
		"\"players\": { \"max\": 4, \"online\": 0, \"sample\": [] },"
		"\"description\": { \"text\": \"description\" } }");
	if (n >= json_len - 1) {
		fprintf(stderr, "attempted to write %d bytes to JSON buffer, consider increasing length\n", n);
		return -1;
	}
	write_string(&p, n, json);
	free(json);

	return write_packet(sfd, finalize_packet(&p));
}

int login_start(int sfd, char username[]) {
	struct recv_packet *p = malloc(sizeof(struct recv_packet));
	if (parse_packet(p, sfd) < 0)
		return -1;
	int len;
	if ((len = read_string(p, username)) > 16) {
		fprintf(stderr, "login_start: expected username to be 16 characters, got %d\n", len);
		return -1;
	}
	free(p);
	return 0;
}

int encryption_request(int sfd, size_t der_len, const unsigned char *der, uint8_t verify[4]) {
	struct send_packet *p = malloc(sizeof(struct send_packet));
	make_packet(p, 0x01);

	char server_id[] = "                    "; // kinda dumb but ok
	write_string(p, 20, server_id);

	write_varint(p, der_len);
	for (size_t i = 0; i < der_len; ++i)
		write_byte(p, der[i]);

	/* verify token */
	write_varint(p, 4);
	for (int i = 0; i < 4; ++i) {
		write_byte(p, (verify[i] = (rand() % 255)));
	}

	// TODO: take packet pointer to write to and return b so caller can write to socket themselves
	int b = write_packet(sfd, finalize_packet(p));
	free(p);
	return b;
}

int decrypt_byte_array(struct recv_packet *p, EVP_PKEY_CTX *ctx, size_t len, uint8_t *out) {
	int bytes;
	if (read_varint(p, &bytes) < 0 || bytes != 128) {
		return -1;
	}
	uint8_t encrypted[128];
	for (int i = 0; i < 128; ++i)
		encrypted[i] = read_byte(p);
	int n;
	if ((n = EVP_PKEY_decrypt(ctx, out, &len, encrypted, 128)) < 0) {
		fprintf(stderr, "EVP_PKEY_decrypt: %lu", ERR_get_error());
		return -1;
	}
	return n;
}

int encryption_response(int sfd, EVP_PKEY_CTX *ctx, const uint8_t verify[4], uint8_t secret[16]) {
	struct recv_packet *p = malloc(sizeof(struct recv_packet));
	if (parse_packet(p, sfd) < 0)
		return -1;

	const size_t buf_len = 1000;
	uint8_t *buf = malloc(sizeof(uint8_t) * buf_len);
	if (decrypt_byte_array(p, ctx, buf_len, buf) < 0) {
		return -1;
	}
	for (int i = 0; i < 16; ++i) {
		secret[i] = buf[i];
	}

	/* read client verify */
	if (decrypt_byte_array(p, ctx, buf_len, buf) < 0) {
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
	free(p);
	return 0;
}

int login_success(struct conn *c, const char uuid[36], const char username[16]) {
	struct send_packet p = {0};
	make_packet(&p, 0x02);
	write_string(&p, 36, uuid);
	write_string(&p, 16, username);
	return conn_write_packet(c, finalize_packet(&p));
}

int ping(int sfd, uint8_t l[8]) {
	struct recv_packet p = {0};
	if (parse_packet(&p, sfd) < 0)
		return -1;

	for (int i = 0; i < 8; ++i)
		l[i] = read_byte(&p);
	return 0;
}

int pong(int sfd, uint8_t l[8]) {
	struct send_packet p = {0};
	make_packet(&p, 0x01);

	for (int i = 0; i < 8; ++i)
		write_byte(&p, l[i]);
	return write_packet(sfd, finalize_packet(&p));
}

int join_game(struct conn *c) {
	struct send_packet p = {0};
	make_packet(&p, 0x26);

	/* TODO: keep track of EID for each player */
	write_int(&p, 123);
	/* gamemode */
	write_byte(&p, 1);
	/* dimension */
	write_int(&p, 0);

	/* TODO: pass a valid SHA-256 hash */
	write_long(&p, 0);

	/* max players, ignored */
	write_byte(&p, 0);

	char level_type[16] = "default";
	write_string(&p, 16, level_type);

	/* view distance */
	write_varint(&p, 10);

	/* reduced debug info */
	write_byte(&p, false);

	/* enable respawn screen */
	write_byte(&p, true);

	return conn_write_packet(c, finalize_packet(&p));
}

int client_settings(struct conn *c) {
	struct recv_packet p = {0};
	if (conn_parse_packet(c, &p) < 0)
		return -1;

	char locale[17] = {0};
	if (read_string(&p, locale) < 0)
		return -1;
	puts(locale);

	uint8_t view_distance = read_byte(&p);
	printf("view distance: %d\n", view_distance);

	int chat_mode;
	if (read_varint(&p, &chat_mode) < 0)
		return -1;
	printf("chat mode: %d\n", chat_mode);

	uint8_t chat_colors = read_byte(&p);
	printf("chat colors: %d\n", chat_colors);

	uint8_t displayed_skin = read_byte(&p);
	printf("displayed skin shit: %d\n", displayed_skin);

	int main_hand;
	if (read_varint(&p, &main_hand) < 0)
		return -1;
	printf("main hand: %d\n", main_hand);

	return 0;

}

int window_items(struct conn *c) {
	struct send_packet p = {0};
	make_packet(&p, 0x15);

	/* window ID */
	write_byte(&p, 0);
	/* slot count */
	write_short(&p, 0);
	/* TODO: slot array https://wiki.vg/Slot_Data */

	return conn_write_packet(c, finalize_packet(&p));
}

int held_item_change_clientbound(struct conn *c, uint8_t slot) {
	struct send_packet p = {0};
	make_packet(&p, 0x40);

	write_byte(&p, slot);
	return conn_write_packet(c, finalize_packet(&p));
}

int spawn_position(struct conn *c, uint16_t x, uint16_t y, uint16_t z) {
	struct send_packet p = {0};
	make_packet(&p, 0x4E);

	/* https://wiki.vg/Protocol#Position */
	uint64_t pos = (((uint64_t)x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF);
	write_long(&p, pos);
	return conn_write_packet(c, finalize_packet(&p));
}

size_t write_section_to_packet(const struct section *s, struct send_packet *p) {
	if (s->bits_per_block == -1)
		return 0;

	/* count non-air blocks */
	uint16_t block_count = 0;
	for (int b = 0; b < TOTAL_BLOCKSTATES; ++b)
		if (s->blockstates[b] != 0)
			++block_count;
	write_short(p, block_count);

	/* palette */
	const uint8_t bits_per_block = s->bits_per_block;
	write_byte(p, bits_per_block);
	uint8_t palette_len = s->palette_len;
	write_varint(p, palette_len);
	for (int i = 0; i < palette_len; ++i)
		write_varint(p, s->palette[i]);

	/* write the blocks */
	size_t blockstates_len = BLOCKSTATES_LEN(s->bits_per_block);
	write_varint(p, blockstates_len);
	for (size_t b = 0; b < blockstates_len; ++b)
		write_long(p, s->blockstates[b]);

	return p->_packet_len;
}

int chunk_data(struct conn *c, const struct chunk *chunk, int x, int y, bool full) {
	struct send_packet p = {0};
	make_packet(&p, 0x22);

	write_int(&p, x);
	write_int(&p, y);
	write_byte(&p, full);

	/* primary bit mask */
	int section_bit_mask = 0;
	for (int i = 0; i < chunk->sections_len; ++i) {
		int has_blocks = chunk->sections[i]->bits_per_block > 0;
		section_bit_mask |= (has_blocks << i);
	}
	write_varint(&p, section_bit_mask);

	struct nbt n = {0};
	nbt_write_init(&n, "");
	/* TODO: actually calculate heightmaps */
	int64_t heightmaps[36] = {0};
	nbt_write_long_array(&n, "MOTION_BLOCKING", 36, heightmaps);
	nbt_finish(&n);
	write_nbt(&p, &n);
	free(n.data);

	if (full)
		for (int i = 0; i < 1024; ++i)
			/* TODO: don't just write desert for every biome xd */
			write_int(&p, 2);

	/* Write each chunk in this[0] format to temporary buffers
	 * and record the total length of each buffer for writing
	 * to the main packet.
	 *    [0]: https://wiki.vg/Chunk_Format#Chunk_Section_structure
	 */
	size_t data_len = 0;
	struct send_packet *block_data = calloc(chunk->sections_len, sizeof(struct send_packet));
	for (int i = 0; i < chunk->sections_len; ++i)
		data_len += write_section_to_packet(chunk->sections[i], &(block_data[i]));

	/* write sections from before */
	write_varint(&p, data_len);
	for (int s = 0; s < chunk->sections_len; ++s)
		for (unsigned int i = 0; i < block_data[s]._packet_len; ++i)
			write_byte(&p, block_data[s]._data[i]);
	free(block_data);

	/* # of block entities */
	/* TODO: implement block entities */
	write_varint(&p, 0);

	return conn_write_packet(c, finalize_packet(&p));
}

int player_position_look(struct conn *c, int *server_teleport_id) {
	struct send_packet p = {0};
	make_packet(&p, 0x36);

	double x = 0;
	double y = 0;
	double z = 0;
	write_double(&p, x);
	write_double(&p, y);
	write_double(&p, z);

	float yaw = 0;
	float pitch = 0;
	write_float(&p, yaw);
	write_float(&p, pitch);

	uint8_t flags = 0;
	write_byte(&p, flags);

	/* TODO: randomize it, probably */
	*server_teleport_id = 123;
	write_varint(&p, *server_teleport_id);

	return conn_write_packet(c, finalize_packet(&p));
}

int teleport_confirm(struct recv_packet *p, int server_teleport_id) {
	int teleport_id;
	if (read_varint(p, &teleport_id) < 0) {
		fprintf(stderr, "reading teleport id failed\n");
		return -1;
	} else if (teleport_id != server_teleport_id) {
		fprintf(stderr, "teleport ID mismatch: %d != %d\n", teleport_id, server_teleport_id);
		return -1;
	}

	return 0;
}

int keep_alive_clientbound(struct conn *c, time_t *t, uint64_t *id) {
	struct send_packet p = {0};
	make_packet(&p, 0x21);

	*id = rand();
	write_long(&p, *id);
	*t = time(NULL);

	return conn_write_packet(c, finalize_packet(&p));
}

int keep_alive_serverbound(struct recv_packet *p, uint64_t id) {
	uint64_t client_id;
	read_long(p, &client_id);
	/* maybe this function isn't the right place for handling the ID mismatch */
	if (client_id != id) {
		fprintf(stderr, "keep alive ID mismatch\n");
		return -1;
	}
	return 0;
}
