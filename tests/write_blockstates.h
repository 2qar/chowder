#include <assert.h>
#include <alloca.h>
#include <stdint.h>

#define CHOWDER_DEBUG
#include "../blockstates.h"

int test_write_blockstates() {
	const uint64_t test_blockstate = 1190112520884487201;
	const int bits_per_block = 5;
	const int in_blockstates_len = (64 / bits_per_block) + 1;
	int *in_blockstates = alloca(sizeof(int) * in_blockstates_len);
	for (int i = 0; i < in_blockstates_len; ++i)
		in_blockstates[i] = 1;

	uint64_t out_blockstate = 0;
	int offset = 0;
	offset = write_blockstate(offset, bits_per_block, &(in_blockstates), &out_blockstate);

	if (out_blockstate != test_blockstate) {
		fprintf(stderr, "write_blockstate failed: expected %lu, got %lu\n",
				test_blockstate, out_blockstate);
	} else {
		printf("%lu == %lu\n", out_blockstate, test_blockstate);
	}
	return 0;
}
