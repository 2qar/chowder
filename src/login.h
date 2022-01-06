#ifndef CHOWDER_LOGIN_H
#define CHOWDER_LOGIN_H

#include <stdint.h>

#include <openssl/evp.h>

#include "conn.h"
#include "packet.h"

struct login_ctx {
	EVP_PKEY_CTX *decrypt_ctx;
	size_t pubkey_len;
	uint8_t *pubkey;
};

int handle_server_list_ping(struct conn *);
int login(struct conn *, struct login_ctx *);

#endif
