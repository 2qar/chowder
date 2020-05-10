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
