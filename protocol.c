#include <stdlib.h>
#include <stdio.h>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#include "protocol.h"

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

	const size_t json_len = 1000;
	char json[json_len];
	/* TODO: don't hardcode, insert state instead (once state exists */
	int n = snprintf(json, json_len-1, "{ \"version\": { \"name\": \"1.15.2\", \"protocol\": 578 },"
		"\"players\": { \"max\": 4, \"online\": 0, \"sample\": [] },"
		"\"description\": { \"text\": \"description\" } }");
	if (n >= json_len - 1) {
		fprintf(stderr, "attempted to write %d bytes to JSON buffer, consider increasing length\n", n);
		return -1;
	}
	write_string(&p, n, json);

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
	for (int i = 0; i < der_len; ++i)
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
	uint8_t buf[buf_len];
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

int ping(struct conn *c, uint8_t l[8]) {
	struct recv_packet p = {0};
	if (conn_parse_packet(c, &p) < 0)
		return -1;

	for (int i = 0; i < 8; ++i)
		l[i] = read_byte(&p);
	return 0;
}

int pong(struct conn *c, uint8_t l[8]) {
	struct send_packet p = {0};
	make_packet(&p, 0x01);

	for (int i = 0; i < 8; ++i)
		write_byte(&p, l[i]);
	return conn_write_packet(c, finalize_packet(&p));
}
