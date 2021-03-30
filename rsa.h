#ifndef CHOWDER_RSA
#define CHOWDER_RSA

int generate_key(EVP_PKEY **pkey);
int rsa_der(EVP_PKEY *pkey, size_t *der_len, uint8_t **der);
EVP_PKEY_CTX *pkey_ctx_init(EVP_PKEY *);

#endif
