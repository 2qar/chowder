#include "blocks.h"
#include "config.h"
#include "conn.h"
#include "login.h"
#include "protocol.h"
#include "rsa.h"
#include "server.h"
#include "strutil.h"
#include "world.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <openssl/evp.h>

#define CONFIG_PATH "server.properties"
#define LEVELS_DIR  "levels"
#define BLOCKS_PATH "gamedata/blocks.json"

#define TICK_LEN_NSEC 50000000

static bool running = true;

void sigint_handler(int);
int bind_socket(uint16_t port, unsigned max_players);

int main()
{
	enum config_err conf_err = read_server_properties(CONFIG_PATH);
	if ((conf_err != CONFIG_OK && conf_err != CONFIG_READ)
	    || (conf_err == CONFIG_READ
		&& set_default_server_properties(CONFIG_PATH) < 0))
		exit(EXIT_FAILURE);
	struct sigaction act = { 0 };
	act.sa_handler = sigint_handler;
	if (sigaction(SIGINT, &act, NULL) < 0)
		perror("sigaction");

	/* socket init */
	int sfd = bind_socket(server_properties.server_port,
			      server_properties.max_players);
	if (sfd < 0)
		exit(EXIT_FAILURE);

	char *level_path = NULL;
	asprintf(&level_path, LEVELS_DIR "/%s", server_properties.level_name);
	if (level_path == NULL) {
		exit(EXIT_FAILURE);
	} else if (access(level_path, R_OK | W_OK | X_OK) < 0) {
		fprintf(stderr, "error accessing \"%s\": %s\n", level_path,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
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

	struct world *w = world_new(level_path, block_table);
	if (w == NULL) {
		free(level_path);
		exit(EXIT_FAILURE);
	} else if (world_load_level_data(w) < 0) {
		world_free(w);
		exit(EXIT_FAILURE);
	}
	struct list *connections = list_new();
	struct packet packet;
	packet_init(&packet);

	/* TODO: keep track of a "tick debt" so the server can catch up when a
	 *       tick takes too long */
	struct timespec tick_start_time;
	struct timespec current_time;
	while (running) {
		if (clock_gettime(CLOCK_MONOTONIC, &tick_start_time) < 0) {
			perror("clock_gettime");
			break;
		}

		int conn = accept(sfd, NULL, NULL);
		if (conn == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
			perror("accept");
		} else if (conn != -1) {
			struct login_ctx l_ctx;
			l_ctx.decrypt_ctx = ctx;
			l_ctx.pubkey_len = der_len;
			l_ctx.pubkey = der;

			struct conn *c =
			    server_accept_connection(conn, &packet, w, &l_ctx);
			if (c != NULL) {
				list_append(connections, sizeof(struct conn *),
					    &c);
			}
		}

		struct list *connection = connections;
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
		connection = connections;
		struct protocol_do_err err = { 0 };
		while (!list_empty(connection)
		       && err.err_type == PROTOCOL_DO_ERR_SUCCESS) {
			struct list *messages =
			    ((struct conn *) list_item(connection))
				->messages_out;
			err = server_send_messages(connections, messages);
			connection = list_next(connection);
		}
		if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
			fprintf(stderr, "error sending message or some shit\n");
		}

		if (clock_gettime(CLOCK_MONOTONIC, &current_time) < 0) {
			perror("clock_gettime");
			break;
		} else if (current_time.tv_nsec - tick_start_time.tv_nsec
			   < TICK_LEN_NSEC) {
			struct timespec sleep_time = { 0 };
			sleep_time.tv_nsec =
			    TICK_LEN_NSEC
			    - (current_time.tv_nsec - tick_start_time.tv_nsec);
			/* TODO: handle interrupted sleep, probably */
			clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep_time, NULL);
		}
	}

	puts("shutdown time");

	free(packet.data);
	free(der);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	close(sfd);
	world_free(w);
	free_server_properties();

	exit(EXIT_SUCCESS);
}

void sigint_handler(int signum)
{
	assert(signum == SIGINT);
	running = false;
}

int bind_socket(uint16_t port, unsigned max_players)
{
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (fcntl(sfd, F_SETFL, O_NONBLOCK) < 0) {
		perror("fcntl");
		return -1;
	}

	struct sockaddr_in saddr = { 0 };
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = 0; // localhost idiot

	if (bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr)) != 0) {
		perror("bind");
		return -1;
	}

	if (listen(sfd, max_players) != 0) {
		perror("listen");
		return -1;
	}

	return sfd;
}
