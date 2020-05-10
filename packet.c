#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "packet.h"

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
uint8_t _read_byte(struct recv_packet *p) {
	return p->_data[p->_index++];
}

// TODO: make _read_varint take a next() function for the next byte instead of copypasta into this function
int read_varint(struct recv_packet *p, int *v) {
	int n = 0;
	*v = 0;
	uint8_t b;
	do {
		b = _read_byte(p);
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
		b[i] = _read_byte(p);
	b[len] = 0;
	return len;
}

void read_ushort(struct recv_packet *p, uint16_t *s) {
	*s = _read_byte(p) << 8;
	*s += _read_byte(p);
}
