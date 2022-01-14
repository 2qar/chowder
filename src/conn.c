#include "conn.h"
#include <stdio.h>
#include <sys/socket.h>
#include "message.h"

int cipher_init(EVP_CIPHER_CTX **ctx, const uint8_t secret[16], int enc) {
	*ctx = EVP_CIPHER_CTX_new();
	return EVP_CipherInit_ex(*ctx, EVP_aes_128_cfb8(), NULL, secret, secret, enc);
}

int conn_init(struct conn *c, int sfd, const uint8_t secret[16]) {
	c->sfd = sfd;
	if (!cipher_init(&(c->_decrypt_ctx), secret, 0))
		return -1;
	if (!cipher_init(&(c->_encrypt_ctx), secret, 1))
		return -1;
	c->messages_out = list_new();
	return 0;
}

void conn_finish(struct conn *c) {
	close(c->sfd);
	EVP_CIPHER_CTX_free(c->_decrypt_ctx);
	EVP_CIPHER_CTX_free(c->_encrypt_ctx);
	if (c->player != NULL)
		player_free(c->player);
	while (!list_empty(c->messages_out)) {
		message_free(list_remove(c->messages_out));
	}
	list_free(c->messages_out);
}

bool read_encrypted_byte(void *src, uint8_t *b) {
	struct conn *c = (struct conn *)src;
	if (!sfd_read_byte((void *) &(c->sfd), b))
		return false;

	int outl = 1;
	uint8_t decrypted[1];
	if (!EVP_CipherUpdate(c->_decrypt_ctx, decrypted, &outl, b, 1)) {
		fprintf(stderr, "error decrypting byte\n");
		return -1;
	}
	*b = decrypted[0];
	return true;
}

int parse_encrypted_packet(struct conn *c) {
	/* TODO: just recv() 5 bytes w/ MSG_PEEK into a packet buf
	 *       instead of calling read()'ing individual bytes like
	 *       read_encrypted_byte() does */
	struct packet *p = c->packet;
	p->packet_mode = PACKET_MODE_READ;

	int packet_len_bytes = read_varint_gen(read_encrypted_byte, (void *) c, &(p->packet_len));
	if (packet_len_bytes < 0) {
		fprintf(stderr, "error reading packet length\n");
		return packet_len_bytes;
	}

	uint8_t in[p->packet_len];
	if (read(c->sfd, in, p->packet_len) < 0) {
		perror("read");
		return -1;
	}
	int outl = MAX_PACKET_LEN;
	int result = EVP_CipherUpdate(c->_decrypt_ctx, p->data, &outl, in, p->packet_len);
	if (result == 0) {
		/* TODO: report openssl errors here and in write_encrypted_packet */
		fprintf(stderr, "decrypt error\n");
		return -1;
	}

	p->index = 0;
	if (packet_read_varint(p, &(p->packet_id)) < 0) {
		fprintf(stderr, "error reading packet id\n");
		return -1;
	}

	return p->packet_len;
}

ssize_t write_encrypted_packet(struct conn *c) {
	struct packet *p = finalize_packet(c->packet);
	if (p == NULL) {
		fprintf(stderr, "couldn't fit the finalized packet in it's buffer\n");
		return -1;
	}

	int out_len = p->packet_len + EVP_CIPHER_CTX_block_size(c->_encrypt_ctx);
	uint8_t out[out_len];
	if (!EVP_CipherUpdate(c->_encrypt_ctx, out, &out_len, p->data, p->packet_len)) {
		fprintf(stderr, "encrypt error\n");
		return -1;
	}

	return write_packet_data(c->sfd, out, out_len);
}

int conn_packet_read_header(struct conn *c) {
	if (c->_decrypt_ctx != NULL)
		return parse_encrypted_packet(c);
	return packet_read_header(c->packet, c->sfd);
}

ssize_t conn_write_packet(struct conn *c) {
	if (c->_encrypt_ctx != NULL)
		return write_encrypted_packet(c);
	return write_packet(c->sfd, finalize_packet(c->packet));
}
