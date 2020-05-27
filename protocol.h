#include <stdint.h>

#include <openssl/evp.h>

#include "conn.h"
#include "packet.h"

int handshake(int);
int login_start(int, char[]);
int encryption_request(int, size_t, const unsigned char *, uint8_t[4]);
int encryption_response(int, EVP_PKEY_CTX *, const uint8_t[4], uint8_t[16]);
int login_success(struct conn *, const char[36], const char[16]);
