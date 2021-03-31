#ifndef CHOWDER_PACKET
#define CHOWDER_PACKET

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "nbt.h"

#define ERR_CONN_CLOSED    -2
#define ERR_BAD_READ       -3

typedef bool (*read_byte_func)(void *src, uint8_t *b);

bool sfd_read_byte(void *sfd, uint8_t *);
int read_varint_gen(read_byte_func, void *src, int *v);

/* FIXME: dynamically allocate packet data as needed
 *        instead of making huge buffers for every packet */
#define MAX_PACKET_LEN 10000*5

struct recv_packet {
	int packet_id;
	int _packet_len;
	uint8_t _data[MAX_PACKET_LEN];
	int _index;
};
int parse_packet(struct recv_packet *, int sfd);

/* read_byte() and the other primitive reads (read_ushort(), etc.)
 * return false if there's no data left to be read. */
bool read_byte(struct recv_packet *p, uint8_t *);
int read_varint(struct recv_packet *, int *);
int read_string(struct recv_packet *, int buf_len, char *buf);
bool read_ushort(struct recv_packet *, uint16_t *);
bool read_long(struct recv_packet *, uint64_t *);
bool read_position(struct recv_packet *, int32_t *x, int16_t *y, int32_t *z);

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
void write_bytes_direct(struct send_packet *, size_t len, void *);
void write_short(struct send_packet *, int16_t);
int write_varint(struct send_packet *, int);
void write_string(struct send_packet *, int, const char[]);
void write_int(struct send_packet *, int32_t);
void write_float(struct send_packet *, float);
void write_double(struct send_packet *, double);
void write_long(struct send_packet *, uint64_t);
void write_nbt(struct send_packet *, struct nbt *);

#endif
