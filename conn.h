#ifndef CHOWDER_CONN
#define CHOWDER_CONN

#include <stdint.h>

#include <openssl/evp.h>

#include "packet.h"

struct conn {
	int _sfd;
	EVP_CIPHER_CTX *_decrypt_ctx;
	EVP_CIPHER_CTX *_encrypt_ctx;
};

void conn_init(struct conn *, int);
int conn_encrypt_init(struct conn *, const uint8_t[16]);
void conn_finish(struct conn *);
int parse_encrypted_packet(struct conn *, struct recv_packet *);
int write_encrypted_packet(struct conn *, const struct send_packet *);

#endif
