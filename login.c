#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "json.h"
#include "login.h"
#include "protocol.h"

int handle_server_list_ping(struct conn *c) {
	/* handle the empty request packet */
	if (conn_packet_read_header(c) < 0) {
		return -1;
	}

	if (server_list_ping(c) < 0)
		return -1;

	uint8_t l[8] = {0};
	if (ping(c, l) < 0)
		return -1;
	return pong(c, l);
}

char *mc_hash(size_t der_len, const uint8_t *der, const uint8_t secret[16]) {
	SHA_CTX c = {0};
	if (!SHA1_Init(&c)) {
		fprintf(stderr, "SHA1_init(): %lu\n", ERR_get_error());
		return NULL;
	}
	uint8_t server_id[20];
	for (int i = 0; i < 20; ++i) {
		server_id[i] = 32; // ASCII space
	}
	int success = 1;
	success &= SHA1_Update(&c, server_id, 20);
	success &= SHA1_Update(&c, secret, 16);
	success &= SHA1_Update(&c, der, der_len);
	if (!success) {
		fprintf(stderr, "SHA1_Update failed :(\n");
		return NULL;
	}
	uint8_t sum[SHA_DIGEST_LENGTH];
	if (!SHA1_Final(sum, &c)) {
		fprintf(stderr, "SHA1_Final(): %lu\n", ERR_get_error());
		return NULL;
	}

	BIGNUM *bn = BN_bin2bn(sum, SHA_DIGEST_LENGTH, NULL);
	int negative = BN_is_bit_set(bn, 159);
	if (negative) {
		const int len = BN_num_bytes(bn);
		unsigned char flipped[len];
		BN_bn2bin(bn, flipped);
		for (int i = 0; i < len; ++i)
			flipped[i] = ~flipped[i];
		BN_bin2bn(flipped, len, bn);
		BN_add_word(bn, 1);
	}
	char *hex = BN_bn2hex(bn);
	int hex_len = strlen(hex);
	char *hash = malloc(sizeof(char) * hex_len + 2);
	if (negative) {
		hash[0] = '-';
	}

	int index = negative;
	int num_found = 0;
	for (int i = 0; i < hex_len; ++i) {
		if (num_found || hex[i] != '0') {
			hash[index++] = hex[i];
			num_found = 1;
		}
	}
	hash[index] = 0;

	free(hex);
	BN_free(bn);
	return hash;
}

void ssl_cleanup(SSL_CTX *ctx, SSL *ssl) {
	SSL_free(ssl);
	SSL_CTX_free(ctx);
}

int ping_sessionserver(const char *username, const char *hash, char **response) {
	struct addrinfo hints = {0};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *res;
	int err = getaddrinfo("sessionserver.mojang.com", "www", &hints, &res);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(err));
		return -1;
	} else if (res == NULL) {
		fprintf(stderr, "no results found by getaddrinfo()\n");
		return -1;
	}
	int sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}
	struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
	addr->sin_port = htons(443);
	if (connect(sfd, res->ai_addr, res->ai_addrlen) == -1) {
		perror("connect");
		return -1;
	}

	SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_method());
	if (ssl_ctx == NULL) {
		fprintf(stderr, "SSL_CTX_new(): %ld\n", ERR_get_error());
		return -1;
	}
	SSL *ssl = SSL_new(ssl_ctx);
	if (ssl == NULL) {
		fprintf(stderr, "SSL_new(): %ld\n", ERR_get_error());
		SSL_CTX_free(ssl_ctx);
		return -1;
	}
	if (!SSL_set_fd(ssl, sfd)) {
		fprintf(stderr, "SSL_set_fd(): %ld\n", ERR_get_error());
		ssl_cleanup(ssl_ctx, ssl);
		return -1;
	}

	err = SSL_connect(ssl);
	if (err <= 0) {
		fprintf(stderr, "SSL_connect(): %d\n", SSL_get_error(ssl, err));
		char err_str[256];
		ERR_error_string(ERR_get_error(), err_str);
		printf("error: %s\n", err_str);
		ssl_cleanup(ssl_ctx, ssl);
		return -1;
	}
	freeaddrinfo(res);

	const size_t buf_len = 2048;
	*response = malloc(sizeof(char) * buf_len);
	int write_len = snprintf(*response, buf_len, "GET /session/minecraft/hasJoined?username=%s&serverId=%s HTTP/1.1\r\nHost: sessionserver.mojang.com\r\nUser-Agent: Chowder :)\r\n\r\n", username, hash);
	err = SSL_write(ssl, *response, write_len);
	if (err <= 0) {
		fprintf(stderr, "SSL_write(): %d\n", SSL_get_error(ssl, err));
		ssl_cleanup(ssl_ctx, ssl);
		return -1;
	} else if (err != write_len) {
		fprintf(stderr, "write mismatch! %d != %d\n", err, write_len);
		ssl_cleanup(ssl_ctx, ssl);
		return -1;
	}

	int n = SSL_read(ssl, *response, buf_len);
	if (n <= 0) {
		fprintf(stderr, "SSL_read(): %d\n", SSL_get_error(ssl, n));
		ssl_cleanup(ssl_ctx, ssl);
		return -1;
	} else if ((size_t)n >= buf_len) {
		fprintf(stderr, "response 2 big: n > %ld\n", buf_len);
		ssl_cleanup(ssl_ctx, ssl);
		return -1;
	}
	(*response)[n] = 0;

	SSL_shutdown(ssl);
	ssl_cleanup(ssl_ctx, ssl);
	close(sfd);
	return err;
}

char *read_body(char *resp) {
	/* TODO: don't just ignore HTTP status and Content-Length */
	char *body_start = strstr(resp, "\r\n\r\n");
	if (body_start != NULL)
		return body_start + 4;
	return NULL;
}

static bool property_equal(void *property, void *search_name) {
	struct json_value *name = json_get(property, "name");
	return name->type == JSON_STRING && !strcmp(name->string, search_name);
}

int player_id(const char *hash, char uuid[36], struct player *player) {
	char *response = NULL;
	int response_len = ping_sessionserver(player->username, hash, &response);
	if (response_len < 0) {
		if (response != NULL)
			free(response);
		return -1;
	}
	char *body = read_body(response);
	if (body == NULL)
		return -1;
	struct json_value *root;
	struct json_err_ctx json_err = json_parse(body, &root);
	if (json_err.type != JSON_OK) {
		fprintf(stderr, "error parsing sessionserver json response\n");
		return -1;
	}
	struct json_value *id = json_get(root, "id");
	if (id == NULL) {
		fprintf(stderr, "no id :(\n");
	} else {
		memcpy(uuid, id->string, 32);
	}
	struct json_value *properties = json_get(root, "properties");
	if (properties == NULL) {
		fprintf(stderr, "no properties :(\n");
	} else {
		struct node *texture_node = list_find(properties->array, property_equal, "textures");
		if (texture_node == NULL || list_item(texture_node) == NULL || list_empty(texture_node)) {
			fprintf(stderr, "no textures :(\n");
		} else {
			struct json_value *texture_value = json_get(list_item(texture_node), "value");
			if (texture_value == NULL) {
				fprintf(stderr, "no texture value fuck this\n");
			} else {
				player->textures = strdup(texture_value->string);
			}
		}
	}
	json_free(root);
	free(response);
	if (uuid[0] == 0) {
		fprintf(stderr, "no \"id\" field present in sessionserver response\n");
		return -1;
	}
	return 0;
}

/* return a UUID formatted with dashes (kinda dumb implementation but it works xd) */
void format_uuid(char uuid[32], char formatted[37]) {
	strncat(formatted, uuid, 8);
	formatted[8] = '-';
	strncat(formatted, uuid+8, 4);
	formatted[13] = '-';
	strncat(formatted, uuid+12, 4);
	formatted[18] = '-';
	strncat(formatted, uuid+16, 4);
	formatted[23] = '-';
	strncat(formatted, uuid+20, 12);
}

/* convert a UUID string to network-order bytes */
void uuid_bytes(char uuid[33], uint8_t bytes[16]) {
	BIGNUM *bn = BN_new();
	BN_hex2bn(&bn, uuid);
	assert(BN_num_bytes(bn) == 16);
	BN_bn2bin(bn, bytes);
	BN_free(bn);
}

int login(struct conn *c, struct login_ctx *l_ctx) {
	c->player = calloc(1, sizeof(struct player));
	if (login_start(c, c->player->username) < 0)
		return -1;
	uint8_t verify[4];
	if (encryption_request(c, l_ctx->pubkey_len, l_ctx->pubkey, verify) < 0)
		return -1;
	uint8_t secret[16];
	if (encryption_response(c, l_ctx->decrypt_ctx, verify, secret) < 0)
		return -1;
	char *hash = mc_hash(l_ctx->pubkey_len, l_ctx->pubkey, secret);
	if (!hash) {
		fputs("error generating SHA1 hash", stderr);
		return -1;
	}
	char uuid[33] = {0};
	if (player_id(hash, uuid, c->player) < 0)
		return -1;
	free(hash);

	/* FIXME: rename conn_init() -> conn_crypto_init(), and only use it to
	 *        initialize the encrypt / decrypt contexts */
	if (conn_init(c, c->sfd, secret) < 0) {
		fprintf(stderr, "error initializing encryption\n");
		return -1;
	}

	char formatted_uuid[37] = {0};
	format_uuid(uuid, formatted_uuid);
	uuid_bytes(uuid, c->player->uuid);
	return login_success(c, formatted_uuid, c->player->username);
}
