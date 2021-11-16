#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "read_region.h"
#include "region.h"

int verify_blockstates(struct section *s) {
	int err = 0;
	for (int y = 0; y < 16; ++y) {
		for (int z = 0; z < 16; ++z) {
			for (int x = 0; x < 16; ++x) {
				int blockstate = read_blockstate_at(s, x, y, z);
				if (blockstate < 0) {
					fprintf(stderr, "blockstate %d < 0\n", blockstate);
					err = 1;
				} else if (blockstate >= s->palette_len) {
					fprintf(stderr, "blockstate %d > %d\n", blockstate, s->palette_len);
					err = 1;
				}
			}
		}
	}

	return err;
}

int verify_chunk(struct chunk *c) {
	int err = 0;
	for (int i = 0; i < c->sections_len; ++i) {
		struct section *s = c->sections[i];
		if (s->bits_per_block > 0 && s->blockstates != NULL)
			err |= verify_blockstates(s);
	}

	return err;
}

void test_read_region(struct hashmap *block_table) {
	FILE *f = fopen("r.0.0.mca", "r");

	struct region *region = malloc(sizeof(struct region));
	size_t chunk_len = 0;
	Bytef *chunk_data = NULL;
	for (int z = 0; z < 32; ++z) {
		for (int x = 0; x < 32; ++x) {
			ssize_t n = read_chunk(f, x, z, &chunk_len, &chunk_data);
			if (n < 0) {
				fprintf(stderr, "error reading chunk @ (%d, %d)\n", x, z);
				exit(EXIT_FAILURE);
			} else if (n > 0) {
				struct chunk *c = parse_chunk(block_table, chunk_len, chunk_data);
				if (verify_chunk(c) > 0)
					exit(EXIT_FAILURE);
				region->chunks[z][x] = c;
			}
		}
	}


	free(chunk_data);
	fclose(f);
}
