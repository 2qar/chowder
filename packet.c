#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "packet.h"

#define FINISHED_PACKET_ID 255

bool packet_buf_read_byte(void *p, uint8_t *b) {
	return packet_read_byte((struct recv_packet *) p, b);
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
	uint8_t b;
	do {
		if (!rb(src, &b))
			return (int8_t) b;
		*v |= ((((int32_t) b) & 0x7f) << (7 * n++));
	} while ((b & 0x80) != 0);

	/* FIXME: check that only 5 bytes are read, because that's the max len
	 *        https://wiki.vg/Protocol#VarInt_and_VarLong */

	return n;
}

int read_varint_sfd(int sfd, int *v) {
	return read_varint_gen(sfd_read_byte, (void *)(&sfd), v);
}

int packet_read_header(struct recv_packet *p, int sfd) {
	if (read_varint_sfd(sfd, &(p->_packet_len)) < 0)
		return p->_packet_len;
	int id_len;
	if ((id_len = read_varint_sfd(sfd, &(p->packet_id))) < 0)
		return p->packet_id;

	int n;
	if ((n = read(sfd, p->_data, p->_packet_len-id_len)) <= 0) {
		return -1;
	}

	p->_index = 0;
	return n;
}

bool packet_read_byte(struct recv_packet *p, uint8_t *b) {
	if (p->_index + 1 == MAX_PACKET_LEN)
		return false;

	*b = p->_data[p->_index++];
	return true;
}

int packet_read_varint(struct recv_packet *p, int *v) {
	return read_varint_gen(&packet_buf_read_byte, (void *) p, v);
}

int packet_read_string(struct recv_packet *p, int buf_len, char *buf) {
	int len;
	if (packet_read_varint(p, &len) < 0)
		return len;

	int i = 0;
	while (i < buf_len && i < len && packet_read_byte(p, (uint8_t *) &(buf[i])))
		++i;

	if (i != len)
		return -1;

	buf[i] = 0;
	return i;
}

bool packet_read_short(struct recv_packet *p, uint16_t *s) {
	uint8_t b;
	if (!packet_read_byte(p, &b))
		return false;
	*s = ((uint16_t) b) << 8;
	if (!packet_read_byte(p, &b))
		return false;
	*s += b;
	return true;
}

bool packet_read_long(struct recv_packet *p, uint64_t *l) {
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
bool packet_read_position(struct recv_packet *p, int32_t *x, int16_t *y, int32_t *z) {
	uint64_t l;
	if (!packet_read_long(p, &l))
		return false;

	*x = l >> 38;
	*y = l & 0xFFF;
	*z = (l << 26 >> 38);
	return true;
}

void make_packet(struct send_packet *p, int id) {
	p->_packet_len = 0;
	p->_packet_id = id;
	packet_write_byte(p, id);
}

/* insert packet ID + length at the start of the packet's data buffer. */
struct send_packet *finalize_packet(struct send_packet *p) {
	if (p->_packet_id == FINISHED_PACKET_ID)
		return p;

	int temp_len = p->_packet_len;
	int offset = 0;
	while (temp_len != 0) {
		temp_len >>= 7;
		++offset;
	}
	if (p->_packet_len + offset > MAX_PACKET_LEN)
		return NULL;
	for (int i = p->_packet_len + offset; i >= offset; --i)
		p->_data[i] = p->_data[i-offset];

	temp_len = p->_packet_len;
	p->_packet_len = 0;
	p->_packet_len = temp_len + packet_write_varint(p, temp_len);
	p->_packet_id = FINISHED_PACKET_ID;
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

ssize_t write_packet(int sfd, const struct send_packet *p) {
	if (p == NULL)
		return -1;
	return write_packet_data(sfd, p->_data, p->_packet_len);
}

void packet_write_byte(struct send_packet *p, uint8_t b) {
	/* TODO: make _data a pointer and realloc(3) when
	 *       p->_packet_len exceeds the buffer length */
	p->_data[p->_packet_len++] = b;
}

/* FIXME: hacky as fuck, assumes the pc is little-endian
 *        and relies on casting other types to uint8_t *
 */
void packet_write_bytes(struct send_packet *p, uint8_t *b, int n) {
	for (int i = n - 1; i >= 0; --i)
		packet_write_byte(p, b[i]);
}

/* writes bytes without changing the byte order of the data */
void packet_write_bytes_direct(struct send_packet *p, size_t len, void *data) {
	memcpy(p->_data + p->_packet_len, data, len);
	p->_packet_len += len;
}

void packet_write_short(struct send_packet *p, int16_t s) {
	uint16_t ns = htons(s);
	memcpy(p->_data + p->_packet_len, &ns, sizeof(uint16_t));
	p->_packet_len += sizeof(uint16_t);
}

int packet_write_varint(struct send_packet *p, int i) {
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

void packet_write_string(struct send_packet *p, int len, const char s[]) {
	packet_write_varint(p, len);
	for (int i = 0; i < len; ++i) {
		packet_write_byte(p, s[i]);
	}
}

void packet_write_int(struct send_packet *p, int32_t i) {
	uint32_t ni = htonl(i);
	memcpy(p->_data + p->_packet_len, &ni, 4);
	p->_packet_len += 4;
}

/* FIXME: packet_write_float and packet_write_double assume this pc is little endian */
void packet_write_float(struct send_packet *p, float f) {
	packet_write_bytes(p, (uint8_t *) &f, sizeof(float));
}

void packet_write_double(struct send_packet *p, double d) {
	packet_write_bytes(p, (uint8_t *) &d, sizeof(double));
}

void packet_write_long(struct send_packet *p, uint64_t l) {
	packet_write_bytes(p, (uint8_t *) &l, sizeof(uint64_t));
}

void packet_write_nbt(struct send_packet *p, struct nbt *n) {
	uint8_t *n_data;
	size_t n_len = nbt_pack(n, &n_data);
	packet_write_bytes_direct(p, n_len, n_data);
}
