#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "common.h"
#include "update_light.h"

bool sky_light_arrays_equal(const struct update_light *p1, const struct update_light *p2)
{
	assert(p1->sky_light_mask == p2->sky_light_mask);

	int i = 0;
	while (p1->sky_light_mask >> i) {
		if (p1->sky_light_mask >> i & 1) {
			if (!(p2->sky_light_mask >> i & 1))
				return false;
			if (p1->sky_light_arrays[i].bytes_len != p2->sky_light_arrays[i].bytes_len)
				return false;
			if (memcmp(p1->sky_light_arrays[i].bytes, p2->sky_light_arrays[i].bytes,
						p1->sky_light_arrays[i].bytes_len))
				return false;
		}
		++i;
	}

	return true;
}

bool block_light_arrays_equal(const struct update_light *p1, const struct update_light *p2)
{
	assert(p1->block_light_mask == p2->block_light_mask);

	int i = 0;
	while (p1->block_light_mask >> i) {
		if (p1->block_light_mask >> i & 1) {
			if (!(p2->block_light_mask >> i & 1))
				return false;
			if (p1->block_light_arrays[i].bytes_len != p2->block_light_arrays[i].bytes_len)
				return false;
			if (memcmp(p1->block_light_arrays[i].bytes, p2->block_light_arrays[i].bytes,
						p1->block_light_arrays[i].bytes_len))
				return false;
		}
		++i;
	}

	return true;
}

bool update_light_equal(const struct update_light *p1, const struct update_light *p2)
{
	return p1->chunk_x == p2->chunk_x
		&& p1->chunk_z == p2->chunk_z
		&& p1->sky_light_mask == p2->sky_light_mask
		&& p1->block_light_mask == p2->block_light_mask
		&& p1->empty_sky_light_mask == p2->empty_sky_light_mask
		&& p1->empty_block_light_mask == p2->empty_block_light_mask
		&& sky_light_arrays_equal(p1, p2)
		&& block_light_arrays_equal(p1, p2);
}

int main()
{
	struct update_light light_packet;
	light_packet.chunk_x = 5;
	light_packet.chunk_z = 6;
	light_packet.sky_light_mask = 3;
	light_packet.block_light_mask = 3;
	light_packet.empty_sky_light_mask = 0x3ffff - 3;
	light_packet.empty_block_light_mask = 0x3ffff - 3;

	size_t bytes_len = 6;
	uint8_t bytes[6] = {0};
	struct update_light_sky_light sky_light_arrays[] = {
		{ .bytes_len = bytes_len, .bytes = bytes },
		{ .bytes_len = bytes_len, .bytes = bytes },
	};
	struct update_light_block_light block_light_arrays[] = {
		{ .bytes_len = bytes_len, .bytes = bytes },
		{ .bytes_len = bytes_len, .bytes = bytes },
	};

	light_packet.sky_light_arrays = sky_light_arrays;
	light_packet.block_light_arrays = block_light_arrays;

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_update_light(t.conn->packet, &light_packet);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	test_read_init(&t, PACKET_FILE_PATH);

	if (!conn_packet_read_header(t.conn)) {
		fprintf(stderr, "failed to read header?\n");
		return 1;
	}
	struct update_light *read_light_packet = NULL;
	r = protocol_read_update_light(t.conn->packet, &read_light_packet);
	bool equal = update_light_equal(&light_packet, read_light_packet);
	free(read_light_packet);
	if (!equal) {
		fprintf(stderr, "read chunk differs from provided chunk\n");
		return 1;
	}

	printf("%s\n", PACKET_FILE_PATH);
	test_cleanup(&t);
	return 0;
}
