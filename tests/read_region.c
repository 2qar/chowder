#include <stdio.h>
#include <stdlib.h>

#include "read_region.h"
#include "../region.h"

int verify_blockstates(int blockstates[BLOCKSTATES_LEN], int palette_len) {
	for (int i = 0; i < BLOCKSTATES_LEN; ++i) {
		if (blockstates[i] > palette_len) {
			fprintf(stderr, "blockstate @ %d (%d) > palette_len (%d), probably fucked up\n",
					i, blockstates[i], palette_len);
			return 1;
		}
	}
	return 0;
}

int test_read_region() {
	FILE *f = fopen("r.0.0.mca", "r");

	struct region r = {0};
	size_t chunk_len = 0;
	Bytef *chunk_data = NULL;
	for (int y = 0; y < 32; ++y) {
		for (int x = 0; x < 32; ++x) {
			ssize_t n = read_chunk(f, x, y, &chunk_len, &chunk_data);
			if (n < 0) {
				fprintf(stderr, "error reading chunk @ (%d, %d)\n", x, y);
				return 1;
			} else if (n > 0) {
				printf("parsing chunk (%d,%d)\n", x, y);
				struct chunk *c = parse_chunk(chunk_data);

				for (int i = 0; i < c->sections_len; ++i) {
					if (c->sections[i]->palette_len > 0 && c->sections[i]->blockstates != NULL)
						if (verify_blockstates(c->sections[i]->blockstates, c->sections[i]->palette_len) > 0)
							return 1;
				}

				r.chunks[y][x] = c;
			}
		}
	}

	free(chunk_data);
	free_region(&r);
	fclose(f);
	return 0;
}
