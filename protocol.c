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

int encryption_request(int sfd, size_t der_len, unsigned char *der, uint8_t verify[4]) {
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
	int b = write_packet(sfd, p);
	free(p);
	return b;
}

int decrypt_byte_array(struct recv_packet *p, EVP_PKEY_CTX *ctx, size_t len, uint8_t *out) {
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

int encryption_response(int sfd, EVP_PKEY_CTX *ctx, uint8_t verify[4], uint8_t secret[16]) {
	struct recv_packet *p = malloc(sizeof(struct recv_packet));
	if (parse_packet(p, sfd) < 0)
		return -1;

	int secret_len;
	if (read_varint(p, &secret_len) < 0 && secret_len != 128)
		return -1;
	secret_len = 1000;
	uint8_t client_secret[secret_len];
	if (decrypt_byte_array(p, ctx, (size_t) secret_len, client_secret) < 0) {
		return -1;
	}

	int verify_len;
	if (read_varint(p, &verify_len) < 0)
		return -1;
	uint8_t *client_verify = malloc(sizeof(uint8_t) * verify_len);
	if (decrypt_byte_array(p, ctx, (size_t) verify_len, client_verify) < 0) {
		return -1;
	}
	printf("client verify: %d-%d-%d-%d\n", client_verify[0], client_verify[1], client_verify[2], client_verify[3]);
	printf("verify: %d-%d-%d-%d\n", verify[0], verify[1], verify[2], verify[3]);

	/* verify the two verify byte arrays */
	for (int i = 0; i < 4; ++i) {
		if (client_verify[i] != verify[i]) {
			fputs("mistmatched verify", stderr);
			return -1;
		}
	}

	/* TODO: SHA1 hexdigest of the server id, shared secret, and encoded public key.
	 *       Then, send it to the mojang url and FUCK it's a JSON response
	SHA_CTX *c;
	if (SHA1_Init(c)
	*/

	free(p);
	return 0;
}
