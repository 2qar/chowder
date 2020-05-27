#include <stdio.h>
#include <stdint.h>

#include "conn.h"

void conn_init(struct conn *c, int sfd) {
	c->_sfd = sfd;
}

int cipher_init(EVP_CIPHER_CTX **ctx, const uint8_t secret[16], int enc) {
	*ctx = EVP_CIPHER_CTX_new();
	return EVP_CipherInit_ex(*ctx, EVP_aes_128_cfb8(), NULL, secret, secret, enc);
}

int conn_encrypt_init(struct conn *c, const uint8_t secret[16]) {
	if (!cipher_init(&(c->_decrypt_ctx), secret, 0))
		return -1;
	if (!cipher_init(&(c->_encrypt_ctx), secret, 1))
		return -1;
	return 0;
}

void conn_finish(struct conn *c) {
	close(c->_sfd);
	EVP_CIPHER_CTX_free(c->_decrypt_ctx);
	EVP_CIPHER_CTX_free(c->_encrypt_ctx);
}

int parse_encrypted_packet(struct conn *c, struct recv_packet *p) {
	return -1;
}

int write_encrypted_packet(struct conn *c, const struct send_packet *p) {
	int out_len = p->_packet_len + EVP_CIPHER_CTX_block_size(c->_encrypt_ctx);
	uint8_t out[out_len];
	if (!EVP_CipherUpdate(c->_encrypt_ctx, out, &out_len, p->_data, p->_packet_len)) {
		fprintf(stderr, "encrypt error\n");
		return -1;
	}

	return write_packet_data(c->_sfd, out, out_len);
}
