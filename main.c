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
#include "conn.h"

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
	if (!(ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL))) {
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
		if (next_state == 1) {
			handle_server_list_ping(conn);
			close(conn);
			/* TODO: clean up, dummy */
			return 0;
		}

		struct conn c = {0};
		EVP_PKEY_CTX *login_decrypt_ctx = EVP_PKEY_CTX_dup(ctx);
		int err = login(conn, &c, der, DER_KEY_LEN, login_decrypt_ctx);
		if (err < 0) {
			// TODO: return meaningful errors instead of -1 everywhere
			fprintf(stderr, "error logging in: %d\n", err);
			exit(1);
		}

		join_game(&c);
		puts("joined the game");
		//window_items(&c);

		for (int y = 0; y < 7; ++y)
			for (int x = 0; x < 7; ++x)
				chunk_data(&c, x, y, true);;
		puts("wrote the chunks");

		if (spawn_position(&c, 0, 0, 0) < 0) {
			fprintf(stderr, "error sending spawn position\n");
			exit(1);
		}
		int teleport_id;
		if (player_position_look(&c, &teleport_id) < 0) {
			fprintf(stderr, "error sending position + look\n");
			exit(1);
		}

		puts("sent all of the shit, just waiting on a teleport confirm");

		struct recv_packet p = {0};
		for (;;) {
			if (conn_parse_packet(&c, &p) < 0)
				break;
			switch (p.packet_id) {
				case 0x00:
					printf("teleport confirm: %d\n", teleport_confirm(&p, teleport_id));
					break;
				default:
					printf("unimplemented packet %02x\n", p.packet_id);
					break;
			}
		}

		EVP_PKEY_CTX_free(login_decrypt_ctx);
		conn_finish(&c);
	} else {
		perror("accept");
	}

	free(der);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	close(sfd);
	return 0;
}
