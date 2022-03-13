#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "chunk_data.h"

#define BIOMES_LEN 1024
#define BIOME_PLAINS 1
#define BLOCKS_PER_SECTION 4096

bool heightmaps_equal(struct nbt *n1, struct nbt *n2)
{
	assert(n1->tag == TAG_Compound && n2->tag == n1->tag);
	struct nbt *m1 = nbt_get(n1, TAG_Long_Array, "MOTION_BLOCKING");
	struct nbt *m2 = nbt_get(n2, TAG_Long_Array, "MOTION_BLOCKING");
	assert(m1 != NULL && m2 != NULL);
	bool equal = m1->data.array->len == m2->data.array->len &&
		!memcmp(m1->data.array->data.bytes, m2->data.array->data.bytes, m1->data.array->len * sizeof(int64_t));
	return equal;
}

bool sections_equal(size_t s_len, struct chunk_data_chunk_section *s1, struct chunk_data_chunk_section *s2)
{
	size_t i = 0;
	bool equal = true;
	while (i < s_len && equal) {
		equal = s1[i].block_count == s2[i].block_count &&
			s1[i].bits_per_block == s2[i].bits_per_block &&
			s1[i].palette_len == s2[i].palette_len &&
			!memcmp(s1[i].palette, s2[i].palette, s1[i].palette_len * sizeof(int32_t)) &&
			s1[i].data_array_len == s2[i].data_array_len &&
			!memcmp(s1[i].data_array, s2[i].data_array, s1[i].data_array_len * sizeof(int64_t));
		++i;
	}
	return equal;
}

bool chunk_equal(struct chunk_data *c1, struct chunk_data *c2)
{
	return c1->chunk_x == c2->chunk_x &&
		c1->chunk_z == c2->chunk_z &&
		c1->full_chunk == c2->full_chunk &&
		c1->primary_bit_mask == c2->primary_bit_mask &&
		heightmaps_equal(c1->heightmaps, c2->heightmaps) &&
		sections_equal(1, c1->data, c2->data);
}

int main()
{
	struct chunk_data_chunk_section section = {0};
	section.block_count = 16*16;
	section.bits_per_block = 4;
	section.palette_len = 2;
	section.palette = malloc(sizeof(int32_t) * 2);
	section.palette[0] = 0;
	section.palette[1] = 420;
	section.data_array_len = BLOCKS_PER_SECTION / ((sizeof(int64_t) * 8) / section.bits_per_block);
	section.data_array = calloc(section.data_array_len, sizeof(int64_t));

	section.data_array[0]  = 0x1111111111111111;
	section.data_array[1]  = 0x1111111111111111;
	section.data_array[2]  = 0x1111111111111111;
	section.data_array[3]  = 0x1111111111111111;
	section.data_array[4]  = 0x1111111111111111;
	section.data_array[5]  = 0x1111111111111111;
	section.data_array[6]  = 0x1111111111111111;
	section.data_array[7]  = 0x1111111111111111;
	section.data_array[8]  = 0x1111111111111111;
	section.data_array[9]  = 0x1111111111111111;
	section.data_array[10] = 0x1111111111111111;
	section.data_array[11] = 0x1111111111111111;
	section.data_array[12] = 0x1111111111111111;
	section.data_array[13] = 0x1111111111111111;
	section.data_array[14] = 0x1111111111111111;
	section.data_array[15] = 0x1111111111111111;

	struct chunk_data chunk = {0};
	chunk.chunk_x = 2;
	chunk.chunk_z = 5;
	chunk.full_chunk = true;
	chunk.primary_bit_mask = 0x1;
	chunk.heightmaps = nbt_new(TAG_Long_Array, "MOTION_BLOCKING");
	struct nbt *motion_blocking = nbt_get(chunk.heightmaps, TAG_Long_Array, "MOTION_BLOCKING");
	motion_blocking->data.array = malloc(sizeof(struct nbt_array));
	motion_blocking->data.array->type = TAG_Long_Array;
	motion_blocking->data.array->len = 36;
	motion_blocking->data.array->data.longs = calloc(36, sizeof(int64_t));
	chunk.biomes = malloc(sizeof(int32_t) * BIOMES_LEN);
	for (size_t i = 0; i < BIOMES_LEN; ++i)
		chunk.biomes[i] = BIOME_PLAINS;
	chunk.data_len = 1;
	chunk.data = &section;

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_chunk_data(t.conn->packet, &chunk);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	test_read_init(&t, PACKET_FILE_PATH);

	if (!conn_packet_read_header(t.conn)) {
		fprintf(stderr, "failed to read header?\n");
		return 1;
	}
	struct chunk_data *read_chunk = NULL;
	r = protocol_read_chunk_data(t.conn->packet, &read_chunk);
	bool equal = chunk_equal(&chunk, read_chunk);
	free(read_chunk);
	if (!equal) {
		fprintf(stderr, "read chunk differs from provided chunk\n");
		return 1;
	}

	printf("%s\n", PACKET_FILE_PATH);
	test_cleanup(&t);
	return 0;
}
