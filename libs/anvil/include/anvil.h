#ifndef CHOWDER_ANVIL_H
#define CHOWDER_ANVIL_H

#include "chunk.h"
#include "hashmap.h"
#include "region.h"

#include <stdio.h>

#include <zlib.h>

#define ANVIL_DATA_VERSION 2230

enum anvil_err {
	ANVIL_OK,
	ANVIL_CHUNK_MISSING,
	ANVIL_READ_ERROR,
	ANVIL_ZLIB_ERROR,
	ANVIL_BAD_NBT,
	ANVIL_BAD_CHUNK,
	ANVIL_BAD_DATA_VERSION,
	ANVIL_BAD_RANGE, /* chunk range spans more than one region */
};

struct anvil_get_chunks_ctx {
	FILE *region_file;
	struct hashmap *block_table;
	int cx1, cz1;
	int cx2, cz2;
	int err_x, err_z;
	int missing;
};

enum anvil_err anvil_read_chunk(FILE *region_file, int x, int z,
				size_t *chunk_buf_len, Bytef **chunk,
				size_t *out_len);
enum anvil_err anvil_parse_chunk(struct hashmap *block_table,
				 size_t chunk_data_len, uint8_t *chunk_data,
				 struct chunk **out);

/* anvil_get_chunk() and anvil_get_chunks() take chunk coordinates within the
 * region they're in. They're equivalent to calling anvil_read_chunk() and
 * anvil_parse_chunk(), except they handle the buffer junk for you. */
enum anvil_err anvil_get_chunk(FILE *region_file, struct hashmap *block_table,
			       int x, int z, struct chunk **out);
/* Get all chunks in the range (cx1,cz1) -> (cx2,cz2), inclusive, assuming
 * those two chunks are in the same region. Only gets chunks that haven't
 * been loaded in yet. */
enum anvil_err anvil_get_chunks(struct anvil_get_chunks_ctx *, struct region *);

#endif // CHOWDER_ANVIL_H
