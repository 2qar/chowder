#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "read_region.h"
#include "../region.h"
#include "../blockstates.h"

int verify_blockstates(int blockstates[BLOCKSTATES_LEN], int palette_len, int x, int y, int s) {
	for (int i = 0; i < BLOCKSTATES_LEN; ++i) {
		if (blockstates[i] > palette_len) {
			fprintf(stderr, "chunk (%d, %d) section %d: blockstate @ %d (%d) > palette_len (%d), probably fucked up\n",
					x, y, s, i, blockstates[i], palette_len);
			return 1;
		}
	}
	return 0;
}

int verify_chunk(struct chunk *c, int x, int y) {
	for (int i = 0; i < c->sections_len; ++i) {
		struct section *s = c->sections[i];
		if (s->palette_len > 0 && s->blockstates != NULL)
			if (verify_blockstates(s->blockstates, s->palette_len, x, y, i) > 0)
				return 1;
	}

	return 0;
}

/* TODO: replace w/ a function for comparing w/ given blockstates
 *       instead of just printing blockstates to stdout and
 *       diff-ing with a set of correct values
 */
void print_written_blockstates(struct section *s) {
	uint64_t *out = NULL;
	size_t out_len = network_blockstates(s, &out);
	assert(out_len == 320);
	for (size_t i = 0; i < out_len; ++i)
		printf("%" PRIi64 "\n", out[i]);
}

void test_read_region(struct region **region) {
	FILE *f = fopen("r.0.0.mca", "r");

	*region = malloc(sizeof(struct region));
	size_t chunk_len = 0;
	Bytef *chunk_data = NULL;
	for (int y = 0; y < 32; ++y) {
		for (int x = 0; x < 32; ++x) {
			ssize_t n = read_chunk(f, x, y, &chunk_len, &chunk_data);
			if (n < 0) {
				fprintf(stderr, "error reading chunk @ (%d, %d)\n", x, y);
				exit(EXIT_FAILURE);
			} else if (n > 0) {
				struct chunk *c = parse_chunk(chunk_data);
				if (verify_chunk(c, x, y) > 0)
					exit(EXIT_FAILURE);
				(*region)->chunks[y][x] = c;
			}
		}
	}


	free(chunk_data);
	fclose(f);
}

void read_and_verify_region() {
	struct region *r = NULL;
	test_read_region(&r);
	/* TODO: verify every section */
	print_written_blockstates(r->chunks[0][0]->sections[1]);
	free_region(r);
}
