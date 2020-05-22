#include <stdint.h>

char *mc_hash(size_t der_len, const uint8_t *der, const uint8_t secret[16]);
int player_id(const char *, const char *, char[36]);
