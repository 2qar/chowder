#ifndef CHOWDER_CONN
#define CHOWDER_CONN

#include <stdint.h>

#include <openssl/evp.h>

#include "packet.h"
#include "player.h"

struct conn {
	int sfd;
	struct packet *packet;
	EVP_CIPHER_CTX *_decrypt_ctx;
	EVP_CIPHER_CTX *_encrypt_ctx;
	struct player *player;
};

int conn_init(struct conn *, int, const uint8_t[16]);
void conn_finish(struct conn *);
int conn_packet_read_header(struct conn *);
ssize_t conn_write_packet(struct conn *);

#endif
