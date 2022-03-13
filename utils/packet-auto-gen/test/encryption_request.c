#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "encryption_request.h"

bool packet_equal(struct encryption_request *p1, struct encryption_request *p2)
{
	return !strncmp(p1->server_id, p2->server_id, 20)
		&& p1->pubkey_len == p2->pubkey_len
		&& !memcmp(p1->pubkey, p2->pubkey, p1->pubkey_len)
		&& p1->verify_token_len == p2->verify_token_len
		&& !memcmp(p1->verify_token, p2->verify_token, p1->verify_token_len);
}

int main()
{
	struct encryption_request e;
	e.server_id = strdup("this doesnt matter!!");
	e.pubkey_len = 5;
	e.pubkey = calloc(e.pubkey_len, sizeof(int8_t));
	e.verify_token_len = 4;
	e.verify_token = calloc(e.verify_token_len, sizeof(int8_t));

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_encryption_request(t.conn->packet, &e);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	test_read_init(&t, PACKET_FILE_PATH);

	if (!conn_packet_read_header(t.conn)) {
		fprintf(stderr, "failed to read header?\n");
		return 1;
	}
	struct encryption_request *read_e = NULL;
	r = protocol_read_encryption_request(t.conn->packet, &read_e);
	bool equal = packet_equal(&e, read_e);
	free(read_e);
	if (!equal) {
		fprintf(stderr, "read encryption_request differs from actual encryption_request\n");
		return 1;
	}

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}
