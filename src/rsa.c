#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

int generate_key(EVP_PKEY **pkey)
{
	EVP_PKEY_CTX *ctx;
	if (!(ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL))) {
		/* TODO: maybe return a meaningful error and print it instead of
		 * printing here */
		fprintf(stderr, "EVP_PKEY_CTX_new_id(): %lu\n",
			ERR_get_error());
		return 1;
	}
	if (EVP_PKEY_keygen_init(ctx) <= 0) {
		fprintf(stderr, "EVP_PKEY_keygen_init(): %lu\n",
			ERR_get_error());
		return 1;
	}
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 1024) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_set_rsa_keygen_bits(): %lu\n",
			ERR_get_error());
		return 1;
	}
	if (EVP_PKEY_keygen(ctx, pkey) <= 0) {
		fprintf(stderr, "EVP_PKEY_keygen(): %lu\n", ERR_get_error());
		return 1;
	}
	EVP_PKEY_CTX_free(ctx);
	return 0;
}

int rsa_der(EVP_PKEY *pkey, size_t *der_len, uint8_t **der)
{
	int len = i2d_PUBKEY(pkey, NULL);
	*der = malloc(len + 1);
	int n = i2d_PUBKEY(pkey, der);
	if (n < 0) {
		fprintf(stderr, "i2d_PUBKEY(): %lu\n", ERR_get_error());
		return 1;
	}
	*der -= n;
	(*der)[len] = 0;
	*der_len = len;
	return 0;
}

EVP_PKEY_CTX *pkey_ctx_init(EVP_PKEY *pkey)
{
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, ENGINE_get_default_RSA());
	// TODO: EVP_PKEY_CTX isn't thread-safe, so if i ever get there,
	//       do this decryption context stuff when decrypting packets
	if (ctx == NULL) {
		fprintf(stderr, "EVP_PKEY_CTX_new(): %lu\n", ERR_get_error());
		return NULL;
	}
	if (EVP_PKEY_decrypt_init(ctx) <= 0) {
		fprintf(stderr, "EVP_PKEY_decrypt_init(): %lu\n",
			ERR_get_error());
		return NULL;
	}
	if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_set_rsa_padding(): %lu\n",
			ERR_get_error());
		return NULL;
	}
	return ctx;
}
