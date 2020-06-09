#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "packet.h"

#define FINISHED_PACKET_ID 255

ssize_t packet_read_byte(void *p) {
	return (ssize_t) read_byte((struct recv_packet *) p);
}

ssize_t sfd_read_byte(void *sfd) {
	uint8_t b;
	if (read(*((int *) sfd), &b, (size_t) 1) < 0)
		return -1;
	else
		return (ssize_t) b;
}

int read_varint_gen(ssize_t (*read_byte)(void *), void *src, int *v) {
	int n = 0;
	*v = 0;
	uint8_t b;
	do {
		if ((b = (*read_byte)(src)) < 0)
			return -1;
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

int write_packet_data(int sfd, const uint8_t data[], size_t len) {
	int n;
	if ((n = write(sfd, data, len)) < 0) {
		perror("write");
		return -1;
	} else if (n != len) {
		fprintf(stderr, "whole packet not written: %d != %ld\n", n, len);
		return -1;
	}
	return n;
}

int write_packet(int sfd, const struct send_packet *p) {
	if (p == NULL)
		return -1;
	return write_packet_data(sfd, p->_data, p->_packet_len);
}

void write_byte(struct send_packet *p, uint8_t b) {
	p->_data[p->_packet_len++] = b;
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
