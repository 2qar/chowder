#ifndef CHOWDER_ANVIL_H
#define CHOWDER_ANVIL_H

#include <stdio.h>
#include <zlib.h>
#include "chunk.h"
#include "hashmap.h"

enum anvil_err {
	ANVIL_OK,
	ANVIL_CHUNK_MISSING,
	ANVIL_READ_ERROR,
	ANVIL_ZLIB_ERROR,
	ANVIL_BAD_NBT,
	ANVIL_BAD_CHUNK,
};

struct anvil_get_chunks_ctx {
	FILE *region_file;
	struct hashmap *block_table;
	int x1, z1;
	int x2, z2;
	int err_x, err_z;
};

enum anvil_err anvil_read_chunk(FILE *region_file, int x, int z, size_t *chunk_buf_len, Bytef **chunk, size_t *out_len);
enum anvil_err anvil_parse_chunk(struct hashmap *block_table, size_t chunk_data_len, uint8_t *chunk_data, struct chunk **out);

/* anvil_get_chunk() and anvil_get_chunks() take chunk coordinates within the
 * region they're in. They're equivalent to calling anvil_read_chunk() and
 * anvil_parse_chunk(), except they handle the buffer junk for you. */
enum anvil_err anvil_get_chunk(FILE *region_file, struct hashmap *block_table, int x, int z, struct chunk **out);
/* assumes (x1,z1) and (x2,z2) are in the same region. */
enum anvil_err anvil_get_chunks(struct anvil_get_chunks_ctx *, struct chunk *out[32][32]);

#endif // CHOWDER_ANVIL_H
