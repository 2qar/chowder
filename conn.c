#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>

#include "conn.h"

int cipher_init(EVP_CIPHER_CTX **ctx, const uint8_t secret[16], int enc) {
	*ctx = EVP_CIPHER_CTX_new();
	return EVP_CipherInit_ex(*ctx, EVP_aes_128_cfb8(), NULL, secret, secret, enc);
}

int conn_init(struct conn *c, int sfd, const uint8_t secret[16]) {
	c->_sfd = sfd;
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
	/* TODO: maybe store this in the conn (pretty sure it wont change xd) */
	int block_size = EVP_CIPHER_CTX_block_size(c->_decrypt_ctx);
	int in_len = MAX_PACKET_LEN + block_size;
	uint8_t in[in_len];
	int n = recv(c->_sfd, &in, in_len, MSG_PEEK);
	if (n < 0) {
		perror("read");
		return -1;
	}

	int outl = MAX_PACKET_LEN;
	int result = EVP_CipherUpdate(c->_decrypt_ctx, p->_data, &outl, in, n);
	if (result == 0) {
		/* TODO: report openssl errors here and in write_encrypted_packet */
		fprintf(stderr, "decrypt error\n");
		return -1;
	}

	/* read packet length + id, maybe move to a function in packet.c */
	p->_index = 0;
	n = read_varint(p, &(p->_packet_len));
	if (n < 0)
		return -1;
	if (read_varint(p, &(p->packet_id)) < 0)
		return -1;

	/* pop the parsed packet off the read buffer */
	size_t read_len = p->_packet_len + n + block_size;
	read(c->_sfd, &in, read_len);
	return p->_packet_len;
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

int conn_parse_packet(struct conn *c, struct recv_packet *p) {
	if (c->_decrypt_ctx != NULL)
		return parse_encrypted_packet(c, p);
	return parse_packet(p, c->_sfd);
}

int conn_write_packet(struct conn *c, const struct send_packet *p) {
	if (c->_encrypt_ctx != NULL)
		return write_encrypted_packet(c, p);
	return write_packet(c->_sfd, p);
}
