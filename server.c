#include <stdint.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#include "server.h"

char *mc_hash(size_t der_len, const uint8_t *der, const uint8_t secret[16]) {
	SHA_CTX c = {0};
	if (!SHA1_Init(&c)) {
		fprintf(stderr, "SHA1_init(): %lu\n", ERR_get_error());
		return NULL;
	}
	uint8_t server_id[20];
	for (int i = 0; i < 20; ++i) {
		server_id[i] = 32; // ASCII space
	}
	int success = 1;
	success &= SHA1_Update(&c, server_id, 20);
	success &= SHA1_Update(&c, secret, 16);
	success &= SHA1_Update(&c, der, der_len);
	if (!success) {
		fprintf(stderr, "SHA1_Update failed :(\n");
		return NULL;
	}
	uint8_t sum[SHA_DIGEST_LENGTH];
	if (!SHA1_Final(sum, &c)) {
		fprintf(stderr, "SHA1_Final(): %lu\n", ERR_get_error());
		return NULL;
	}

	BIGNUM *bn = BN_bin2bn(sum, SHA_DIGEST_LENGTH, NULL);
	int negative = BN_is_bit_set(bn, 159);
	if (negative) {
		const int len = BN_num_bytes(bn);
		unsigned char flipped[len];
		BN_bn2bin(bn, flipped);
		for (int i = 0; i < len; ++i)
			flipped[i] = ~flipped[i];
		BN_bin2bn(flipped, len, bn);
		BN_add_word(bn, 1);
	}
	char *hex = BN_bn2hex(bn);
	int hex_len = strlen(hex);
	char *hash = malloc(sizeof(char) * hex_len + 2);
	if (negative) {
		hash[0] = '-';
	}

	int index = negative;
	int num_found = 0;
	for (int i = 0; i < hex_len; ++i) {
		if (num_found || hex[i] != '0') {
			hash[index++] = hex[i];
			num_found = 1;
		}
	}
	hash[index] = 0;

	free(hex);
	BN_free(bn);
	return hash;
}
