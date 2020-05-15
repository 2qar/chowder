#include <stdint.h>

#include <openssl/evp.h>

#include "packet.h"

int handshake(int);
int login_start(int, char[]);
int encryption_request(int, size_t, unsigned char *, uint8_t[4]);
int encryption_response(int, EVP_PKEY_CTX *, uint8_t[4], uint8_t[16]);
