#include <stdlib.h>
#include <stdio.h>

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

int login_start(int sfd, char username[]) {
	struct recv_packet *p = malloc(sizeof(struct recv_packet));
	if (parse_packet(p, sfd) < 0)
		return -1;
	int len;
	if ((len = read_string(p, username)) > 16) {
		fprintf(stderr, "login_start: expected username to be 16 characters, got %d\n", len);
		return -1;
	}
	free(p);
	return 0;
}

int encryption_request(int sfd, size_t pub_key_len, char *pub_key, uint8_t verify[4]) {
	struct send_packet *p = malloc(sizeof(struct send_packet));
	make_packet(p, 0x01);

	char server_id[] = "                    "; // kinda dumb but ok
	write_string(p, 20, server_id);

	write_varint(p, pub_key_len);
	for (int i = 0; i < pub_key_len; ++i)
		write_byte(p, pub_key[i]);

	/* verify token */
	write_varint(p, 4);
	for (int i = 0; i < 4; ++i) {
		write_byte(p, (verify[i] = (rand() % 255)));
	}

	// TODO: take packet pointer to write to and return b so caller can write to socket themselves
	int b = write_packet(sfd, p);
	free(p);
	return b;
}
