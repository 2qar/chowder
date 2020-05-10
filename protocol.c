#include <stdlib.h>

#include "protocol.h"

int handshake(int sfd) {
	struct recv_packet *p = malloc(sizeof(struct recv_packet));
	if (parse_packet(p, sfd) < 0)
		// TODO: maybe use more than -1 for error values
		//       so they make sense instead of all being -1
		return -1;

	int protocol_version;
	if (read_varint(p, &protocol_version) < 0)
		return -1;
	
	char ip[1000];
	if (read_string(p, ip) < 0) {
		return -1;
	}

	uint16_t port;
	read_ushort(p, &port);

	int next_state;
	if (read_varint(p, &next_state) < 0)
		return -1;

	free(p);
	return next_state;
}
