#ifndef CHOWDER_PACKET
#define CHOWDER_PACKET

#include <unistd.h>
#include <stdint.h>

#include "nbt.h"

ssize_t sfd_read_byte(void *sfd);
int read_varint_gen(ssize_t (*read_byte)(void *src), void *src, int *v);

#define MAX_PACKET_LEN 10000

struct recv_packet {
	int packet_id;
	int _packet_len;
	uint8_t _data[MAX_PACKET_LEN];
	int _index;
};
int parse_packet(struct recv_packet *, int sfd);
uint8_t read_byte(struct recv_packet *p);
int read_varint(struct recv_packet *, int *);
int read_string(struct recv_packet *, char[]);
void read_ushort(struct recv_packet *, uint16_t *);
void read_long(struct recv_packet *, uint64_t *);

struct send_packet {
	unsigned int _packet_len;
	uint8_t _packet_id;
	uint8_t _data[MAX_PACKET_LEN];
};
void make_packet(struct send_packet *, int);
struct send_packet *finalize_packet(struct send_packet *);
ssize_t write_packet_data(int, const uint8_t data[], size_t len);
ssize_t write_packet(int, const struct send_packet *);
void write_byte(struct send_packet *, uint8_t);
void write_short(struct send_packet *, int16_t);
int write_varint(struct send_packet *, int);
void write_string(struct send_packet *, int, const char[]);
void write_int(struct send_packet *, int32_t);
void write_float(struct send_packet *, float);
void write_double(struct send_packet *, double);
void write_long(struct send_packet *, uint64_t);
void write_nbt(struct send_packet *, struct nbt *);

#endif
