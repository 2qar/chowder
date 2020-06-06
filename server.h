#include <stdint.h>

#include <openssl/evp.h>

#include "conn.h"

int login(int, struct conn *, const uint8_t *, size_t, EVP_PKEY_CTX *);
int handle_server_list_ping(int);
