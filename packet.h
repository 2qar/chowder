#ifndef CHOWDER_PACKET
#define CHOWDER_PACKET

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "nbt.h"

typedef bool (*read_byte_func)(void *src, uint8_t *b);

/* returns true if reading was successful, otherwise returns false
 * and writes the return value of read() to the given byte */
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
int packet_read_header(struct recv_packet *, int sfd);

/* packet_read_byte() and the other primitive reads (packet_read_ushort(), etc.)
 * return false if there's no data left to be read. */
bool packet_read_byte(struct recv_packet *p, uint8_t *);
int packet_read_varint(struct recv_packet *, int *);
int packet_read_string(struct recv_packet *, int buf_len, char *buf);
bool packet_read_short(struct recv_packet *, uint16_t *);
bool packet_read_long(struct recv_packet *, uint64_t *);
bool packet_read_position(struct recv_packet *, int32_t *x, int16_t *y, int32_t *z);

struct send_packet {
	unsigned int _packet_len;
	uint8_t _packet_id;
	uint8_t _data[MAX_PACKET_LEN];
};
void make_packet(struct send_packet *, int);
struct send_packet *finalize_packet(struct send_packet *);
ssize_t write_packet_data(int, const uint8_t data[], size_t len);
ssize_t write_packet(int, const struct send_packet *);
void packet_write_byte(struct send_packet *, uint8_t);
void packet_write_bytes_direct(struct send_packet *, size_t len, void *);
void packet_write_short(struct send_packet *, int16_t);
int packet_write_varint(struct send_packet *, int);
void packet_write_string(struct send_packet *, int, const char[]);
void packet_write_int(struct send_packet *, int32_t);
void packet_write_float(struct send_packet *, float);
void packet_write_double(struct send_packet *, double);
void packet_write_long(struct send_packet *, uint64_t);
void packet_write_nbt(struct send_packet *, struct nbt *);

#endif
