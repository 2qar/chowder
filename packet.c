#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "packet.h"

#define FINISHED_PACKET_ID 255

ssize_t packet_read_byte(void *p) {
	return (ssize_t) read_byte((struct recv_packet *) p);
}

ssize_t sfd_read_byte(void *sfd) {
	ssize_t b;
	int n = read(*((int *) sfd), &b, (size_t) 1);
	if (n == -1) {
		perror("read");
		return -1;
	} else if (n == 0) {
		return ERR_CONN_CLOSED;
	}
	return b;
}

int read_varint_gen(ssize_t (*read_byte)(void *), void *src, int *v) {
	int n = 0;
	*v = 0;
	/* FIXME: this needs to be a normal, signed byte */
	ssize_t b;
	do {
		if ((b = (*read_byte)(src)) < 0)
			return b;
		*v |= ((b & 0x7f) << (7 * n++));
	} while ((b & 0x80) != 0);

	return n;
}

int read_varint_sfd(int sfd, int *v) {
	return read_varint_gen(sfd_read_byte, (void *)(&sfd), v);
}

int parse_packet(struct recv_packet *p, int sfd) {
	if (read_varint_sfd(sfd, &(p->_packet_len)) < 0)
		return -1;
	int id_len;
	if ((id_len = read_varint_sfd(sfd, &(p->packet_id))) < 0)
		return -1;

	int n;
	if ((n = read(sfd, p->_data, p->_packet_len-id_len)) <= 0) {
		return -1;
	}

	p->_index = 0;
	return n;
}

// TODO: probably check index against packet_len before just returning stuff
uint8_t read_byte(struct recv_packet *p) {
	return p->_data[p->_index++];
}

int read_varint(struct recv_packet *p, int *v) {
	return read_varint_gen(&packet_read_byte, (void *) p, v);
}

// TODO: take buffer length as an argument for safety
int read_string(struct recv_packet *p, char b[]) {
	int len;
	if (read_varint(p, &len) < 0)
		return -1;

	for (int i = 0; i < len; ++i)
		b[i] = read_byte(p);
	b[len] = 0;
	return len;
}

void read_ushort(struct recv_packet *p, uint16_t *s) {
	*s = read_byte(p) << 8;
	*s += read_byte(p);
}

void read_long(struct recv_packet *p, uint64_t *l) {
	uint64_t hl = 0;
	for (int i = 7; i >= 0; --i)
		hl |= read_byte(p) << (i * 8);
	*l = hl;
}

void make_packet(struct send_packet *p, int id) {
	p->_packet_len = 0;
	p->_packet_id = id;
	write_byte(p, id);
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
	p->_packet_len = temp_len + write_varint(p, temp_len);
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

void write_byte(struct send_packet *p, uint8_t b) {
	/* TODO: make _data a pointer and realloc(3) when
	 *       p->_packet_len exceeds the buffer length */
	p->_data[p->_packet_len++] = b;
}

/* FIXME: hacky as fuck, assumes the pc is little-endian
 *        and relies on casting other types to uint8_t *
 */
void write_bytes(struct send_packet *p, uint8_t *b, int n) {
	for (int i = n - 1; i >= 0; --i)
		write_byte(p, b[i]);
}

void write_short(struct send_packet *p, int16_t s) {
	uint16_t ns = htons(s);
	memcpy(p->_data + p->_packet_len, &ns, sizeof(uint16_t));
	p->_packet_len += sizeof(uint16_t);
}

int write_varint(struct send_packet *p, int i) {
	uint8_t temp;
	int n = 0;
	do {
		temp = i & 0x7f;
		i >>= 7;
		if (i != 0) {
			temp |= 0x80;
		}
		write_byte(p, temp);
		++n;
	} while (i != 0);
	return n;
}

void write_string(struct send_packet *p, int len, const char s[]) {
	write_varint(p, len);
	for (int i = 0; i < len; ++i) {
		write_byte(p, s[i]);
	}
}

void write_int(struct send_packet *p, int32_t i) {
	uint32_t ni = htonl(i);
	memcpy(p->_data + p->_packet_len, &ni, 4);
	p->_packet_len += 4;
}

/* FIXME: write_float and write_double assume this pc is little endian */
void write_float(struct send_packet *p, float f) {
	write_bytes(p, (uint8_t *) &f, sizeof(float));
}

void write_double(struct send_packet *p, double d) {
	write_bytes(p, (uint8_t *) &d, sizeof(double));
}

void write_long(struct send_packet *p, uint64_t l) {
	write_bytes(p, (uint8_t *) &l, sizeof(uint64_t));
}

void write_nbt(struct send_packet *p, struct nbt *n) {
	for (int i = 0; i < n->_index; ++i)
		write_byte(p, n->data[i]);
}
