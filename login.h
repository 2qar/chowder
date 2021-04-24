#ifndef CHOWDER_LOGIN_H
#define CHOWDER_LOGIN_H

#include <stdint.h>

#include <openssl/evp.h>

#include "conn.h"

int handle_server_list_ping(struct conn *);
int login(struct conn *, const uint8_t *, size_t, EVP_PKEY_CTX *);

#endif
