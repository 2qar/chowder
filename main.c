#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

#include <assert.h>

#define PLAYERS 4

int read_varint(int);
int read_string(char[], int);

int main() {
	/* socket init */
	int sfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in saddr = {0};
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(25565);
	saddr.sin_addr.s_addr = 0; // localhost idiot

	if (bind(sfd, (struct sockaddr *) &saddr, sizeof(saddr)) != 0) {
		perror("bind");
		exit(1);
	}

	if (listen(sfd, PLAYERS) != 0) {
		perror("listen");
		exit(1);
	}

	/* RSA keygen */
	RSA *rsa;
	if (!(rsa = RSA_new())) {
		fprintf(stderr, "RSA_new: %lu", ERR_get_error());
		exit(1);
	}
	BIGNUM *n;
	if (!(n = BN_new())) {
		fprintf(stderr, "BN_new: %lu", ERR_get_error());
		exit(1);
	}

	/* (hopefully) set n to 3 */
	if (!(BN_set_bit(n, 0))) {
		fprintf(stderr, "BN_set_bit: %lu", ERR_get_error());
		exit(1);
	}
	if (!(BN_set_bit(n, 1))) {
		fprintf(stderr, "BN_set_bit: %lu", ERR_get_error());
		exit(1);
	}

	if (!RSA_generate_key_ex(rsa, 1024, n, NULL)) {
		fprintf(stderr, "RSA_generate_key_ex: %lu", ERR_get_error());
		exit(1);
	}

	int conn;
	if ((conn = accept(sfd, NULL, NULL)) != -1) {
		/* handshake */
		// TODO: make a function that reads packet content into a byte buffer
		//       using the parsed packet_size to avoid making a gajillion syscalls just to parse the packet
		int packet_size = read_varint(conn);
		printf("packet size: %d\n", packet_size);
		int packet_id = read_varint(conn);
		printf("packet ID: %d\n", packet_id);
		assert(packet_id == 0x00);
		int protocol_version = read_varint(conn);
		printf("protocol version: %d\n", protocol_version);
		char ip[1000];
		if (read_string(ip, conn) < 0) {
			perror("read");
			exit(1);
		}
		printf("ip: %s\n", ip);
		uint16_t port;
		// FIXME: handle 0
		if (read(conn, &port, 2) < 0) {
			perror("read");
			exit(1);
		}
		printf("port: %d\n", ntohs(port));
		int next_state = read_varint(conn);
		printf("next state: %d\n", next_state);
	} else {
		perror("accept");
	}

	BN_clear_free(n);
	RSA_free(rsa);
	close(sfd);
}

int read_varint(int sfd) {
	int n = 0;
	int result = 0;
	uint8_t b;
	do {
		// FIXME: handle read == 0
		if (read(sfd, &b, (size_t) 1) < 0) {
			fprintf(stderr, "fuck\n");
			perror("read");
			exit(1);
		}
		result |= ((b & 0x7f) << (7 * n++));
	} while ((b & 0x80) != 0);
	return result;
}

int read_string(char b[], int sfd) {
	int len = read_varint(sfd);
	// FIXME: this probably doesn't actually handle UTF-8 properly but idk
	// FIXME: handle read == 0
	if (read(sfd, b, len) < 0) {
		return -1;
	}
	b[len] = 0;
	return len;
}
