#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#include <openssl/evp.h>

#include <assert.h>

#include "blocks.h"
#include "protocol.h"
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
int handle_connection(struct world *, int conn_fd, EVP_PKEY_CTX *ctx, size_t der_len, const uint8_t *der);

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
	failed = create_block_table(BLOCKS_PATH);
	if (failed)
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

	/* connection handling */
	while (running) {
		int conn = accept(sfd, NULL, NULL);
		if (conn != -1) {
			handle_connection(w, conn, ctx, der_len, der);
		} else if (errno != EINTR) {
			perror("accept");
		}
	}

	puts("shutdown time");

	free(der);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	close(sfd);

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


int handle_connection(struct world *w, int sfd, EVP_PKEY_CTX *ctx, size_t der_len, const uint8_t *der) {
	struct conn conn = {0};
	conn.sfd = sfd;
	conn.packet = malloc(sizeof(struct packet));

	int next_state = handshake(&conn);
	if (next_state == 1) {
		handle_server_list_ping(&conn);
		conn_finish(&conn);
		return 0;
	}

	printf("next state is %d\n", next_state);

	EVP_PKEY_CTX *login_decrypt_ctx = EVP_PKEY_CTX_dup(ctx);
	int err = login(&conn, der, der_len, login_decrypt_ctx);
	if (err < 0) {
		// TODO: return meaningful errors instead of -1 everywhere
		fprintf(stderr, "error logging in: %d\n", err);
		return 1;
	}
	EVP_PKEY_CTX_free(login_decrypt_ctx);

	join_game(&conn);
	puts("joined the game");
	if (client_settings(&conn) < 0) {
		fprintf(stderr, "error reading client settings\n");
		return 1;
	}
	if (held_item_change_clientbound(&conn, 0) < 0) {
		fprintf(stderr, "error sending held item change\n");
		return 1;
	}
	if (window_items(&conn) < 0) {
		fprintf(stderr, "error sending window items\n");
		return 1;
	}

	struct region *r = world_region_at(w, 0, 0);
	if (r == NULL) {
		r = calloc(1, sizeof(struct region));
		world_add_region(w, r);
		puts("### ALLOCATING THE REGION FOR THE FIRST TIME ###");
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
					chunk = parse_chunk(uncompressed_len, chunk_buf);
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
				chunk_data(&conn, chunk, x, z, true);
				r->chunks[z][x] = chunk;
			}
		}
	}
	fclose(f);
	free(chunk_buf);

	if (spawn_position(&conn, 0, 0, 0) < 0) {
		fprintf(stderr, "error sending spawn position\n");
		return 1;
	}
	int teleport_id;
	if (player_position_look(&conn, &teleport_id) < 0) {
		fprintf(stderr, "error sending position + look\n");
		return 1;
	}

	puts("sent all of the shit, just waiting on a teleport confirm");

	struct pollfd pfd = { .fd = sfd, .events = POLLIN };

	uint64_t keep_alive_id;
	time_t keep_alive_time = 0;
	time_t last_client_response = time(NULL);
	for (;;) {
		int polled = poll(&pfd, 1, 100);
		if (polled > 0 && (pfd.revents & POLLIN)) {
			int result = conn_packet_read_header(&conn);
			if (result == 0) {
				puts("client closed connection");
				break;
			} else if (result < 0) {
				fprintf(stderr, "error parsing packet\n");
				break;
			}
			switch (conn.packet->packet_id) {
				case 0x00:
					printf("teleport confirm: %d\n", teleport_confirm(conn.packet, teleport_id));
					break;
				case 0x0F:
					if (keep_alive_serverbound(conn.packet, keep_alive_id) < 0)
						break;
					last_client_response = time(NULL);
					break;
				case 0x2C:
					player_block_placement(conn.packet, w);
					break;
				default:
					//printf("unimplemented packet 0x%02x\n", p.packet_id);
					break;
			}
		} else if (polled < 0) {
			perror("poll");
			break;
		}

		if (time(NULL) - last_client_response >= 30) {
			puts("client hasn't sent a keep alive in a while, disconnecting");
			break;
		}

		if (time(NULL) - keep_alive_time > 15) {
			if (keep_alive_clientbound(&conn, &keep_alive_time, &keep_alive_id) < 0) {
				fprintf(stderr, "error sending keep alive\n");
				break;
			}
		}
	}

	free(conn.packet);
	conn_finish(&conn);
	return 0;
}
