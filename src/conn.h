#ifndef CHOWDER_CONN
#define CHOWDER_CONN

#include "message.h"
#include "packet.h"
#include "player.h"

#include <stdint.h>
#include <time.h>

#include <openssl/evp.h>

struct conn {
	int sfd;
	struct packet *packet;
	EVP_CIPHER_CTX *_decrypt_ctx;
	EVP_CIPHER_CTX *_encrypt_ctx;
	struct player *player;

	uint8_t view_distance;
	int32_t teleport_id;
	int64_t keep_alive_id;
	time_t last_ping;
	time_t last_pong;
	struct list *messages_out;
	bool requesting_chunks; /* true after crossing a chunk border */
	int old_chunk_x;
	int old_chunk_z;
};

int conn_init(struct conn *, int, const uint8_t[16]);
void conn_finish(struct conn *);
int conn_packet_read_header(struct conn *);
ssize_t conn_write_packet(struct conn *);

void conn_update_view_position_if_needed(struct conn *, double new_x,
					 double new_z);

#endif
