#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>

#include "packet.h"

#define FINISHED_PACKET_ID 255

void packet_init(struct packet *p) {
	memset(p, 0, sizeof(struct packet));
	p->data_len = PACKET_BLOCK_SIZE;
	p->data = malloc(PACKET_BLOCK_SIZE);
}

void packet_free(struct packet *p) {
	free(p->data);
	free(p);
}

bool packet_buf_read_byte(void *p, uint8_t *b) {
	return packet_read_byte((struct packet *) p, b);
}

bool sfd_read_byte(void *sfd, uint8_t *b) {
	int n = read(*((int *) sfd), b, 1);
	if (n <= 0) {
		*b = n;
		return false;
	}
	return true;
}

int read_varint_gen(read_byte_func rb, void *src, int *v) {
	int n = 0;
	*v = 0;
	uint8_t b = 0;
	do {
		if (!rb(src, &b))
			return (int8_t) b;
		*v |= ((((int32_t) b) & 0x7f) << (7 * n++));
	} while ((b & 0x80) != 0 && n <= 5);

	if (n > 5)
		return PACKET_VARINT_TOO_LONG;
	return n;
}

int read_varint_sfd(int sfd, int *v) {
	return read_varint_gen(sfd_read_byte, (void *)(&sfd), v);
}

int packet_read_header(struct packet *p, int sfd) {
	p->packet_mode = PACKET_MODE_READ;

	int n = read_varint_sfd(sfd, &(p->packet_len));
	if (n <= 0) {
		return n;
	}
	int id_len = read_varint_sfd(sfd, &(p->packet_id));
	if (id_len <= 0) {
		return n;
	}

	size_t old_data_len = p->data_len;
	while ((size_t)(p->packet_len - id_len) > p->data_len) {
		p->data_len += PACKET_BLOCK_SIZE;
	}
	if (p->data_len > MAX_PACKET_LEN) {
		return PACKET_TOO_BIG;
	} else if (p->data_len != old_data_len) {
		void *buf = realloc(p->data, p->data_len);
		if (buf == NULL) {
			return PACKET_REALLOC_FAILED;
		} else {
			p->data = buf;
		}
	}

	n = read(sfd, p->data, p->packet_len - id_len);
	if (n <= 0) {
		return n;
	}

	p->index = 0;
	return n;
}

bool packet_read_byte(struct packet *p, uint8_t *b) {
	assert(p->packet_mode == PACKET_MODE_READ);

	if (p->index + 1 == MAX_PACKET_LEN) {
		*b = 0;
		return false;
	}

	*b = p->data[p->index++];
	return true;
}

int packet_read_varint(struct packet *p, int *v) {
	return read_varint_gen(&packet_buf_read_byte, (void *) p, v);
}

int packet_read_string(struct packet *p, int buf_len, char *buf) {
	int len;
	if (packet_read_varint(p, &len) < 0)
		return len;

	int i = 0;
	while (i < buf_len - 1 && i < len && packet_read_byte(p, (uint8_t *) &(buf[i])))
		++i;

	if (i != len)
		return -1;

	buf[i] = 0;
	return i;
}

bool packet_read_short(struct packet *p, uint16_t *s) {
	uint8_t b;
	if (!packet_read_byte(p, &b))
		return false;
	*s = ((uint16_t) b) << 8;
	if (!packet_read_byte(p, &b))
		return false;
	*s += b;
	return true;
}

bool packet_read_long(struct packet *p, uint64_t *l) {
	uint64_t hl = 0;
	int i = 7;
	uint8_t b;
	while (i >= 0 && packet_read_byte(p, &b)) {
		hl |= ((uint64_t) b) << (i * 8);
		--i;
	}

	if (i != -1)
		return false;
	*l = hl;
	return true;
}

/* TODO: test w/ negative values if i ever get around to sending chunks w/
 *       negative coordinates xd */
bool packet_read_position(struct packet *p, int32_t *x, int16_t *y, int32_t *z) {
	uint64_t l;
	if (!packet_read_long(p, &l))
		return false;

	*x = l >> 38;
	*y = l & 0xFFF;
	*z = (l << 26 >> 38);
	return true;
}

void make_packet(struct packet *p, int id) {
	p->packet_mode = PACKET_MODE_WRITE;
	p->packet_len = 0;
	p->index = 0;
	p->packet_id = id;
	packet_write_byte(p, id);
}

/* insert packet ID + length at the start of the packet's data buffer. */
struct packet *finalize_packet(struct packet *p) {
	if (p->packet_id == FINISHED_PACKET_ID)
		return p;

	int len = p->packet_len;
	int packet_len_bytes = 0;
	while (len != 0) {
		len >>= 7;
		++packet_len_bytes;
	}

	if (p->packet_len + packet_len_bytes > MAX_PACKET_LEN)
		return NULL;

	memmove(p->data + packet_len_bytes, p->data, p->packet_len);

	len = p->packet_len;
	p->index = 0;
	p->packet_len = len + packet_write_varint(p, len);
	p->packet_id = FINISHED_PACKET_ID;
	return p;
}

ssize_t write_packet_data(int sfd, const uint8_t data[], size_t len) {
	ssize_t n;
	if ((n = write(sfd, data, len)) < 0) {
		perror("write");
		return -1;
	} else if ((size_t)n != len) {
		fprintf(stderr, "whole packet not written: %ld != %ld\n", n, len);
		return -1;
	}
	return n;
}

ssize_t write_packet(int sfd, const struct packet *p) {
	if (p == NULL)
		return -1;
	return write_packet_data(sfd, p->data, p->packet_len);
}

static int packet_try_resize(struct packet *p, size_t new_size) {
	if (new_size > MAX_PACKET_LEN) {
		return PACKET_TOO_BIG;
	} else if (new_size > p->data_len) {
		size_t new_data_len = p->data_len;
		while (new_data_len < new_size) {
			new_data_len += PACKET_BLOCK_SIZE;
		}
		void *buf = realloc(p->data, new_data_len);
		if (buf == NULL) {
			return PACKET_REALLOC_FAILED;
		}
		p->data_len = new_data_len;
		p->data = buf;
	}
	return 0;
}

int packet_write_byte(struct packet *p, uint8_t b) {
	assert(p->packet_mode == PACKET_MODE_WRITE);

	int err = packet_try_resize(p, p->packet_len + 1);
	if (err)
		return err;

	p->data[p->index++] = b;
	++(p->packet_len);
	return 1;
}

/* writes bytes without changing the byte order of the data */
int packet_write_bytes(struct packet *p, size_t len, const void *data) {
	assert(p->packet_mode == PACKET_MODE_WRITE);

	int err = packet_try_resize(p, p->packet_len + len);
	if (err)
		return err;

	memcpy(p->data + p->index, data, len);
	p->index += len;
	p->packet_len += len;
	return len;
}

int packet_write_short(struct packet *p, int16_t s) {
	uint16_t ns = htons(s);
	return packet_write_bytes(p, sizeof(uint16_t), &ns);
}

int packet_write_varint(struct packet *p, int i) {
	uint8_t temp;
	int n = 0;
	do {
		temp = i & 0x7f;
		i >>= 7;
		if (i != 0) {
			temp |= 0x80;
		}
		packet_write_byte(p, temp);
		++n;
	} while (i != 0);
	return n;
}

int packet_write_string(struct packet *p, int len, const char s[]) {
	int len_bytes = packet_write_varint(p, len);
	if (len_bytes < 0)
		return len_bytes;

	int n_bytes = 0;
	int i = 0;
	int n = 1;
	while (n == 1 && i < len) {
		n = packet_write_byte(p, s[i++]);
		n_bytes += n;
	}
	if (i != len) {
		return n;
	} else {
		return len_bytes + n_bytes;
	}
}

int packet_write_int(struct packet *p, int32_t i) {
	uint32_t ni = htonl(i);
	return packet_write_bytes(p, sizeof(uint32_t), &ni);
}

int packet_write_float(struct packet *p, float f) {
	int32_t i;
	memcpy(&i, &f, sizeof(float));
	return packet_write_int(p, i);
}

int packet_write_double(struct packet *p, double d) {
	int64_t i;
	memcpy(&i, &d, sizeof(double));
	return packet_write_long(p, i);
}

int packet_write_long(struct packet *p, uint64_t l) {
	uint64_t nl = htobe64(l);
	return packet_write_bytes(p, sizeof(uint64_t), &nl);
}

int packet_write_nbt(struct packet *p, struct nbt *nbt) {
	uint8_t *nbt_data;
	size_t nbt_len = nbt_pack(nbt, &nbt_data);
	int n = packet_write_bytes(p, nbt_len, nbt_data);
	free(nbt_data);
	return n;
}
