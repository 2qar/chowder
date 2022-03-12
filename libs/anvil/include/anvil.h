#ifndef CHOWDER_ANVIL_H
#define CHOWDER_ANVIL_H

#include "anvil_err.h"
#include "chunk.h"
#include "hashmap.h"
#include "region.h"

#include <stdio.h>

#include <zlib.h>

#define ANVIL_DATA_VERSION 2230

struct anvil_get_chunks_ctx {
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
enum anvil_err anvil_get_chunk(const struct region *,
			       struct hashmap *block_table, int x, int z,
			       struct chunk **out);
/* Get all chunks in the range (cx1,cz1) -> (cx2,cz2), inclusive, assuming
 * those two chunks are in the same region. Only gets chunks that haven't
 * been loaded in yet. */
enum anvil_err anvil_get_chunks(struct anvil_get_chunks_ctx *, struct region *);

#endif // CHOWDER_ANVIL_H
