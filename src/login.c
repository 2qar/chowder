#include "login.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <curl/curl.h>

#include "json.h"
#include "protocol_autogen.h"
#include "protocol.h"

#define WRITE_CALLBACK_CHUNK_SIZE 4096

int handle_server_list_ping(struct conn *conn) {
	/* handle the empty request packet */
	int packet_err = packet_read_header(conn->packet, conn->sfd);
	if (packet_err < 0) {
		fprintf(stderr, "handle_server_list_ping: failed to read packet header: %d\n", packet_err);
		return -1;
	}

	struct server_list_ping status_pack;
	/* TODO: don't hardcode, insert state instead (once state exists) */
	status_pack.status_json = "{ \"version\": { \"name\": \"1.15.2\", \"protocol\": 578 },"
		"\"players\": { \"max\": 4, \"online\": 0, \"sample\": [] },"
		"\"description\": { \"text\": \"description\" } }";
	struct protocol_do_err err = PROTOCOL_WRITE(server_list_ping, conn, &status_pack);
	// FIXME: this error handling sucks, but it could be worse
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "handle_server_list_ping: server_list_ping failed\n");
		return -1;
	}

	struct ping ping;
	PROTOCOL_READ_S(ping, conn, ping, err);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "handle_server_list_ping: ping failed\n");
		return -1;
	}
	struct pong pong = { .payload = ping.payload };
	err = PROTOCOL_WRITE(pong, conn, &pong);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "handle_server_list_ping: pong failed\n");
		return -1;
	}
	return 0;
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

struct write_ctx {
	char *buf;
	size_t buf_len;
	size_t index;
};

static size_t write_callback(char *buf, size_t size, size_t buf_len, void *userdata) {
	assert(size == 1);
	struct write_ctx *ctx = userdata;
	bool realloc_needed = ctx->index + buf_len > ctx->buf_len;
	while (ctx->index + buf_len > ctx->buf_len) {
		ctx->buf_len += WRITE_CALLBACK_CHUNK_SIZE;
	}
	if (realloc_needed) {
		ctx->buf = reallocarray(ctx->buf, ctx->buf_len, sizeof(char));
	}
	memcpy(ctx->buf + ctx->index, buf, buf_len);
	ctx->index += buf_len;
	return buf_len;
}

static CURLcode ping_sessionserver(const char *username, const char *server_id, char **body) {
	const char *uri_base_str = "https://sessionserver.mojang.com/session/minecraft/hasJoined";
	size_t uri_str_len = strlen(uri_base_str) + strlen(username) + strlen(server_id)
		+ strlen("?username=&serverId=") + 1;
	char *uri_str = calloc(uri_str_len, sizeof(char));
	snprintf(uri_str, uri_str_len, "%s?username=%s&serverId=%s", uri_base_str, username, server_id);
	CURL *curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, uri_str);
		struct write_ctx ctx;
		ctx.buf_len = WRITE_CALLBACK_CHUNK_SIZE;
		ctx.buf = calloc(WRITE_CALLBACK_CHUNK_SIZE, sizeof(char));
		ctx.index = 0;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		CURLcode res = curl_easy_perform(curl);
		free(uri_str);
		curl_easy_cleanup(curl);
		*body = ctx.buf;
		return res;
	} else {
		free(uri_str);
		return CURLE_FAILED_INIT;
	}
}

static bool property_equal(void *property, void *search_name) {
	struct json_value *name = json_get(property, "name");
	return name->type == JSON_STRING && !strcmp(name->string, search_name);
}

int player_id(const char *hash, char uuid[36], struct player *player) {
	char *response_body = NULL;
	CURLcode err = ping_sessionserver(player->username, hash, &response_body);
	if (err != CURLE_OK) {
		fprintf(stderr, "failed to ping sessionserver: %s", curl_easy_strerror(err));
		free(response_body);
		return -1;
	}
	struct json_value *root;
	struct json_err_ctx json_err = json_parse(response_body, &root);
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
		struct list *texture_node = list_find(properties->array, property_equal, "textures");
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
	free(response_body);
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

/* frees the encrypted buffer pointed at by *buf, replacing it with the decrypted
 * buffer, and puts the new length in *buf_len */
static int decrypt_bytes(EVP_PKEY_CTX *decrypt_ctx, size_t *buf_len, uint8_t **buf) {
	size_t new_buf_len;
	int err = EVP_PKEY_decrypt(decrypt_ctx, NULL, &new_buf_len, *buf, *buf_len);
	if (err != 1) {
		return err;
	}
	uint8_t *new_buf = malloc(new_buf_len);
	err = EVP_PKEY_decrypt(decrypt_ctx, new_buf, &new_buf_len, *buf, *buf_len);
	if (err != 1) {
		free(new_buf);
		return err;
	} else {
		free(*buf);
		*buf = new_buf;
		*buf_len = new_buf_len;
		return 1;
	}
}

int login(struct conn *c, struct login_ctx *l_ctx) {
	(void) l_ctx;
	c->player = calloc(1, sizeof(struct player));
	struct login_start login_start_pack;
	struct protocol_do_err err;
	PROTOCOL_READ_S(login_start, c, login_start_pack, err);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "login: failed to read login_start packet\n");
		return -1;
	}
	size_t username_len = strlen(login_start_pack.username);
	memcpy(c->player->username, login_start_pack.username, username_len);
	c->player->username[username_len] = '\0';
	uint8_t verify_token[4];
	for (int i = 0; i < 4; ++i) {
		verify_token[i] = rand();
	}
	struct encryption_request encryption_request_pack = {
		.server_id = "                    ",
		.pubkey_len = l_ctx->pubkey_len,
		.pubkey = l_ctx->pubkey,
		.verify_token_len = 4,
		.verify_token = verify_token,
	};
	err = PROTOCOL_WRITE(encryption_request, c, &encryption_request_pack);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "login: failed to send encryption request\n");
		return -1;
	}
	struct encryption_response encryption_response_pack;
	PROTOCOL_READ_S(encryption_response, c, encryption_response_pack, err);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "login: failed to read encryption response\n");
		return -1;
	}
	if (decrypt_bytes(l_ctx->decrypt_ctx,
				(size_t *) &encryption_response_pack.shared_secret_len,
				&encryption_response_pack.shared_secret) <= 0) {
		fprintf(stderr, "login: failed to decrypt shared secret\n");
		return -1;
	} else if (decrypt_bytes(l_ctx->decrypt_ctx,
				(size_t *) &encryption_response_pack.verify_token_len,
				&encryption_response_pack.verify_token) <= 0) {
		fprintf(stderr, "login: failed to decrypt verify token\n");
		return -1;
	} else if (encryption_response_pack.verify_token_len != 4 ||
			memcmp(verify_token, encryption_response_pack.verify_token, 4)) {
		// FIXME: this shouldn't return the same """error""" as serious
		//        errors like decryption failing
		fprintf(stderr, "login: verify_token mismatch\n");
		return -1;
	}
	char *hash = mc_hash(l_ctx->pubkey_len, l_ctx->pubkey, encryption_response_pack.shared_secret);
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
	if (conn_init(c, c->sfd, encryption_response_pack.shared_secret) < 0) {
		fprintf(stderr, "error initializing encryption\n");
		return -1;
	}

	char formatted_uuid[37] = {0};
	format_uuid(uuid, formatted_uuid);
	uuid_bytes(uuid, c->player->uuid);
	struct login_success login_success_pack = {
		.uuid = formatted_uuid,
		.username = c->player->username,
	};
	err = PROTOCOL_WRITE(login_success, c, &login_success_pack);
	if (err.err_type != PROTOCOL_DO_ERR_SUCCESS) {
		fprintf(stderr, "login: error sending login success\n");
		return -1;
	}
	return 0;
}
