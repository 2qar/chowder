#include <stdint.h>

#include <openssl/evp.h>

#include "conn.h"

int login(struct conn *, const uint8_t *, size_t, EVP_PKEY_CTX *);
int handle_server_list_ping(struct conn *);
