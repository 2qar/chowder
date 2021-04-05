#ifndef CHOWDER_PACKET
#define CHOWDER_PACKET

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "nbt.h"

#define PACKET_VARINT_TOO_LONG -2
#define PACKET_TOO_BIG         -3

typedef bool (*read_byte_func)(void *src, uint8_t *b);

/* returns true if reading was successful, otherwise returns false
 * and writes the return value of read() to the given byte */
bool sfd_read_byte(void *sfd, uint8_t *);
int read_varint_gen(read_byte_func, void *src, int *v);

/* https://wiki.vg/Protocol#Packet_format */
#define MAX_PACKET_LEN 2097151
/* default packet buf len + size added each time the buffer fills up */
#define PACKET_BLOCK_SIZE 4096

/* TODO: maybe enforce read / write states to avoid funky business */
/* packets are pretty much a read/write buffer for network data.
 * "packet" might not be the best name for them but it works */
struct packet {
	int packet_id;
	int packet_len;
	size_t data_len;
	uint8_t *data;
	int index;
};

/* allocates the packet's data buffer + zeroes the fields just in case */
void packet_init(struct packet *);
void packet_free(struct packet *);

int packet_read_header(struct packet *, int sfd);
/* packet_read_byte() and the other primitive reads (packet_read_ushort(), etc.)
 * return false if there's no data left to be read. */
bool packet_read_byte(struct packet *p, uint8_t *);
int packet_read_varint(struct packet *, int *);
int packet_read_string(struct packet *, int buf_len, char *buf);
bool packet_read_short(struct packet *, uint16_t *);
bool packet_read_long(struct packet *, uint64_t *);
bool packet_read_position(struct packet *, int32_t *x, int16_t *y, int32_t *z);

void make_packet(struct packet *, int);
struct packet *finalize_packet(struct packet *);
ssize_t write_packet_data(int, const uint8_t data[], size_t len);
ssize_t write_packet(int, const struct packet *);
void packet_write_byte(struct packet *, uint8_t);
void packet_write_bytes(struct packet *, size_t len, void *);
void packet_write_short(struct packet *, int16_t);
int packet_write_varint(struct packet *, int);
void packet_write_string(struct packet *, int, const char[]);
void packet_write_int(struct packet *, int32_t);
void packet_write_float(struct packet *, float);
void packet_write_double(struct packet *, double);
void packet_write_long(struct packet *, uint64_t);
void packet_write_nbt(struct packet *, struct nbt *);

#endif
