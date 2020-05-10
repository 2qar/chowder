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

#include "protocol.h"

#define PLAYERS 4

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
		int next_state = handshake(conn);
		// TODO: handle packet status 1, server list ping
		assert(next_state == 2);
		printf("next_state: %d\n", next_state);
	} else {
		perror("accept");
	}

	BN_clear_free(n);
	RSA_free(rsa);
	close(sfd);
}
