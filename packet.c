#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "packet.h"

#define FINISHED_PACKET_ID 255

int _read_varint(int sfd, int *v) {
	int n = 0;
	*v = 0;
	uint8_t b;
	do {
		// FIXME: handle read == 0
		if (read(sfd, &b, (size_t) 1) < 0) {
			return -1;
		}
		*v |= ((b & 0x7f) << (7 * n++));
	} while ((b & 0x80) != 0);

	return n;
}


int parse_packet(struct recv_packet *p, int sfd) {
	if (_read_varint(sfd, &(p->_packet_len)) < 0)
		return -1;
	int id_len;
	if ((id_len = _read_varint(sfd, &(p->packet_id))) < 0)
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

// TODO: make _read_varint take a next() function for the next byte instead of copypasta into this function
int read_varint(struct recv_packet *p, int *v) {
	int n = 0;
	*v = 0;
	uint8_t b;
	do {
		b = read_byte(p);
		*v |= ((b & 0x7f) << (7 * n++));
	} while ((b & 0x80) != 0);

	return n;
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

int write_packet(int sfd, const struct send_packet *p) {
	if (p == NULL) {
		return -1;
	}

	int n;
	if ((n = write(sfd, p->_data, p->_packet_len)) < 0) {
		perror("write");
		return -1;
	} else if (n != p->_packet_len) {
		fprintf(stderr, "whole packet not written: %d != %d\n", n, p->_packet_len);
		return -1;
	}
	return n;
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

void write_string(struct send_packet *p, int len, char s[]) {
	write_varint(p, len);
	for (int i = 0; i < len; ++i) {
		write_byte(p, s[i]);
	}
}
