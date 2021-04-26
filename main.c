#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#include <openssl/evp.h>

#include <assert.h>

#include "blocks.h"
#include "protocol.h"
#include "login.h"
#include "server.h"
#include "conn.h"
#include "rsa.h"
#include "world.h"

#define PLAYERS    4
#define PORT       25565
#define LEVEL_PATH "levels/default"

#define BLOCKS_PATH "gamedata/blocks.json"

static bool running = true;

void sigint_handler(int);
int check_level_path(char *);
int bind_socket();

int main() {
	struct sigaction act = {0};
	act.sa_handler = sigint_handler;
	if (sigaction(SIGINT, &act, NULL) < 0)
		perror("sigaction");

	/* socket init */
	int sfd = bind_socket();
	if (sfd < 0)
		exit(EXIT_FAILURE);

	/* make sure level exists + load the block table */
	int failed = check_level_path(LEVEL_PATH);
	if (failed)
		exit(EXIT_FAILURE);
	struct hashmap *block_table = create_block_table(BLOCKS_PATH);
	if (block_table == NULL)
		exit(EXIT_FAILURE);

	/* RSA keygen */
	EVP_PKEY *pkey = NULL;
	if (generate_key(&pkey) > 0)
		exit(EXIT_FAILURE);

	uint8_t *der = NULL;
	size_t der_len;
	if (rsa_der(pkey, &der_len, &der) > 0)
		exit(EXIT_FAILURE);

	EVP_PKEY_CTX *ctx = pkey_ctx_init(pkey);
	if (ctx == NULL)
		exit(EXIT_FAILURE);

	struct world *w = world_new();
	w->block_table = block_table;
	struct node *connections = list_new();
	struct packet packet;
	packet_init(&packet);

	/* connection handling */
	while (running) {
		/* FIXME: super high cpu usage, should probably tick */

		int conn = accept(sfd, NULL, NULL);
		if (conn == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
			perror("accept");
		} else if (conn != -1) {
			struct login_ctx l_ctx;
			l_ctx.decrypt_ctx = ctx;
			l_ctx.pubkey_len = der_len;
			l_ctx.pubkey = der;

			struct conn *c = server_accept_connection(conn, &packet, w, &l_ctx);
			if (c != NULL) {
				list_append(connections, sizeof(struct conn *), &c);
			}
		}

		struct node *connection = connections;
		while (!list_empty(connection)) {
			int status = server_play(list_item(connection), w);
			if (status <= 0) {
				struct conn *c = list_remove(connection);
				conn_finish(c);
				free(c);
			} else {
				connection = list_next(connection);
			}
		}
	}

	puts("shutdown time");

	free(packet.data);
	free(der);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	close(sfd);
	world_free(w);

	exit(EXIT_SUCCESS);
}

void sigint_handler(int signum) {
	assert(signum == SIGINT);
	running = false;
}

int check_level_path(char *level_path) {
	/* check if LEVEL_PATH actually exists */
	FILE *level_dir = fopen(level_path, "r");
	if (level_dir == NULL) {
		char err[256];
		snprintf(err, 256, "error opening '%s'", level_path);
		perror(err);
		return 1;
	}
	fclose(level_dir);
	return 0;
}

int bind_socket() {
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (fcntl(sfd, F_SETFL, O_NONBLOCK) < 0) {
		perror("fcntl");
		return -1;
	}

	struct sockaddr_in saddr = {0};
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(PORT);
	saddr.sin_addr.s_addr = 0; // localhost idiot

	if (bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr)) != 0) {
		perror("bind");
		return -1;
	}

	if (listen(sfd, PLAYERS) != 0) {
		perror("listen");
		return -1;
	}

	return sfd;
}
