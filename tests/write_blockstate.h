#include "../region.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void test_write_blockstate_at()
{
	const int64_t expected = 1190112520884487201;

	struct section s = { 0 };
	s.bits_per_block = 5;
	s.blockstates = calloc(2, sizeof(uint64_t));

	const int blockstates_to_write = ceil(64.0 / s.bits_per_block);
	const int blockstate = 1;
	for (int i = 0; i < blockstates_to_write; ++i)
		write_blockstate_at(&s, i, 0, 0, blockstate);

	if ((int64_t) s.blockstates[0] != expected) {
		fprintf(stderr, "expected %lu, got %lu\n", expected,
			s.blockstates[0]);
		exit(EXIT_FAILURE);
	}

	free(s.blockstates);
}
