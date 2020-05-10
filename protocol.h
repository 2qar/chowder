#include <stdint.h>

#include <openssl/bio.h>

#include "packet.h"

int handshake(int);
int login_start(int, char[]);
int encryption_request(int, size_t, char *, uint8_t[4]);
