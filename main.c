#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/engine.h>
#include <openssl/err.h>

#include <assert.h>

#include "protocol.h"
#include "server.h"

#define PLAYERS 4
#define PORT 25566

#define DER_KEY_LEN 162

int main() {
	/* socket init */
	int sfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in saddr = {0};
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(PORT);
	saddr.sin_addr.s_addr = 0; // localhost idiot

	if (bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr)) != 0) {
		perror("bind");
		exit(1);
	}

	if (listen(sfd, PLAYERS) != 0) {
		perror("listen");
		exit(1);
	}

	/* RSA keygen */
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx;
	if ((ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL)) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_new_id(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_keygen_init(ctx) <= 0) {
		fprintf(stderr, "EVP_PKEY_keygen_init(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 1024) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_set_rsa_keygen_bits(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
		fprintf(stderr, "EVP_PKEY_keygen(): %lu\n", ERR_get_error());
		exit(1);
	}
	EVP_PKEY_CTX_free(ctx);

	unsigned char *der = malloc(DER_KEY_LEN + 1);
	int n = i2d_PUBKEY(pkey, &der);
	if (n < 0) {
		fprintf(stderr, "i2d_PUBKEY(): %lu\n", ERR_get_error());
		exit(1);
	} else if (n != DER_KEY_LEN) {
		fprintf(stderr, "len mismatch: %d != %d\n", n, DER_KEY_LEN);
		exit(1);
	}
	der -= n;
	der[DER_KEY_LEN] = 0;

	// TODO: EVP_PKEY_CTX isn't thread-safe, so if i ever get there,
	//       do this decryption context stuff when decrypting packets
	if (!(ctx = EVP_PKEY_CTX_new(pkey, ENGINE_get_default_RSA()))) {
		fprintf(stderr, "EVP_PKEY_CTX_new(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_decrypt_init(ctx) <= 0) {
		fprintf(stderr, "EVP_PKEY_decrypt_init(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_set_rsa_padding(): %lu\n", ERR_get_error());
		exit(1);
	}

	/* connection handling */
	int conn;
	if ((conn = accept(sfd, NULL, NULL)) != -1) {
		int next_state = handshake(conn);
		// TODO: handle packet status 1, server list ping
		assert(next_state == 2);
		printf("next_state: %d\n", next_state);
		char username[17];
		// TODO: handle the error better, dummy
		if (login_start(conn, username) < 0)
			exit(1);
		uint8_t verify[4];
		if (encryption_request(conn, DER_KEY_LEN, der, verify) < 0)
			exit(1);
		uint8_t secret[16];
		if (encryption_response(conn, ctx, verify, secret) < 0)
			exit(1);
		char *hash = mc_hash(DER_KEY_LEN, der, secret);
		if (!hash) {
			fputs("error generating SHA1 hash", stderr);
			exit(1);
		}
		uint8_t id[37];
		if (player_id(username, hash, id) < 0)
			exit(1);
		free(hash);
		close(conn);
	} else {
		perror("accept");
	}

	free(der);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	close(sfd);
	return 0;
}
