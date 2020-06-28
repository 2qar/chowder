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

ssize_t read_encrypted_byte(void *src) {
	struct conn *c = (struct conn *)src;
	ssize_t b = sfd_read_byte((void *) &(c->_sfd));
	if (b < 0)
		return -1;

	int outl = 1;
	uint8_t decrypted[1];
	if (!EVP_CipherUpdate(c->_decrypt_ctx, decrypted, &outl, (uint8_t *) &b, 1)) {
		fprintf(stderr, "error decrypting byte\n");
		return -1;
	}

	return (ssize_t)(decrypted[0]);
}

/* FIXME: this fixes the junk packet issue, but now the client disconnecting causes a segfault */
int parse_encrypted_packet(struct conn *c, struct recv_packet *p) {
	int packet_len_bytes = read_varint_gen(read_encrypted_byte, (void *) c, &(p->_packet_len));
	if (packet_len_bytes < 0) {
		fprintf(stderr, "error reading packet length\n");
		return -1;
	}

	uint8_t in[p->_packet_len];
	if (read(c->_sfd, in, p->_packet_len) < 0) {
		perror("read");
		return -1;
	}
	int outl = MAX_PACKET_LEN;
	int result = EVP_CipherUpdate(c->_decrypt_ctx, p->_data, &outl, in, p->_packet_len);
	if (result == 0) {
		/* TODO: report openssl errors here and in write_encrypted_packet */
		fprintf(stderr, "decrypt error\n");
		return -1;
	}

	p->_index = 0;
	if (read_varint(p, &(p->packet_id)) < 0) {
		fprintf(stderr, "error reading packet id\n");
		return -1;
	}

	return p->_packet_len;
}

ssize_t write_encrypted_packet(struct conn *c, const struct send_packet *p) {
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

ssize_t conn_write_packet(struct conn *c, const struct send_packet *p) {
	if (c->_encrypt_ctx != NULL)
		return write_encrypted_packet(c, p);
	return write_packet(c->_sfd, p);
}
