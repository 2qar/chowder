#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/engine.h>
#include <openssl/err.h>

#include <assert.h>

#include "protocol.h"

#define PLAYERS 4
#define PORT 25566

int main() {
	/* socket init */
	int sfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in saddr = {0};
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(PORT);
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
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx;
	if ((ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL)) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_new_id(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_keygen_init(ctx) <= 0) {
		fprintf(stderr, "EVP_PKEY_keygen_init(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 1024) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_set_rsa_keygen_bits(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
		fprintf(stderr, "EVP_PKEY_keygen(): %lu\n", ERR_get_error());
		exit(1);
	}

	// reading from a BIO into a char * instead of reading directly into char *
	// seems kinda hacky, but it works :)
	// TODO: replace BIO with char *.
	//       i2d_PUBKEY increments the pointer by the # of bytes written (i think),
	//       which is why printing it was printing a bunch of zeroes
	BIO *bio;
	if (!(bio = BIO_new(BIO_s_mem()))) {
		fprintf(stderr, "BIO_new(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (!i2d_PUBKEY_bio(bio, pkey)) {
		fprintf(stderr, "i2d_PUBKEY_fp(): %lu\n", ERR_get_error());
		exit(1);
	}
	int der_len = BIO_pending(bio);
	unsigned char *der = malloc(der_len + 1);
	int n;
	if ((n = BIO_read(bio, der, der_len)) <= 0) {
		fprintf(stderr, "BIO_read(): %lu\n", ERR_get_error());
		exit(1);
	} else if (n != der_len) {
		fprintf(stderr, "BIO_read bytes mismatch: %d != %d", n, der_len);
		exit(1);
	}
	EVP_PKEY_CTX_free(ctx);

	// TODO: EVP_PKEY_CTX isn't thread-safe, so if i ever get there,
	//       do this decryption context stuff when decrypting packets
	if (!(ctx = EVP_PKEY_CTX_new(pkey, ENGINE_get_default_RSA()))) {
		fprintf(stderr, "EVP_PKEY_CTX_new(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_decrypt_init(ctx) <= 0) {
		fprintf(stderr, "EVP_PKEY_decrypt_init(): %lu\n", ERR_get_error());
		exit(1);
	}
	if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
		fprintf(stderr, "EVP_PKEY_CTX_set_rsa_padding(): %lu\n", ERR_get_error());
		exit(1);
	}

	/* connection handling */
	int conn;
	if ((conn = accept(sfd, NULL, NULL)) != -1) {
		int next_state = handshake(conn);
		// TODO: handle packet status 1, server list ping
		assert(next_state == 2);
		printf("next_state: %d\n", next_state);
		char username[17];
		// TODO: handle the error better, dummy
		if (login_start(conn, username) < 0)
			exit(1);
		printf("username: %s\n", username);
		uint8_t verify[4];
		if (encryption_request(conn, der_len, der, verify) < 0)
			exit(1);
		uint8_t secret[16];
		if (encryption_response(conn, ctx, verify, secret) < 0)
			exit(1);
		close(conn);
	} else {
		perror("accept");
	}

	free(der);
	BIO_free(bio);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(pkey);
	close(sfd);
	return 0;
}
