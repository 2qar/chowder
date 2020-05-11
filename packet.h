#include <unistd.h>
#include <stdint.h>

#define MAX_PACKET_LEN 1000

struct recv_packet {
	int packet_id;
	int _packet_len;
	uint8_t _data[1000];
	int _index;
};
int parse_packet(struct recv_packet *, int);
int read_varint(struct recv_packet *, int *);
int read_string(struct recv_packet *, char[]);
void read_ushort(struct recv_packet *, uint16_t *);

struct send_packet {
	unsigned int _packet_len;
	uint8_t _packet_id;
	uint8_t _data[MAX_PACKET_LEN];
};
void make_packet(struct send_packet *, int);
int write_packet(int, struct send_packet *);
void write_byte(struct send_packet *, uint8_t);
int write_varint(struct send_packet *, int);
void write_string(struct send_packet *, int, char[]);
