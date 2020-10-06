#include "blockstates.h"

uint8_t mask_gen(int size) {
	/* probably not necessary xd */
	if (size > 8)
		return 0xff;

	uint8_t mask = 0;
	for (int i = 0; i < size; ++i)
		mask |= 1 << i;
	return mask;
}

int bits_active(uint8_t mask) {
	int active = 0;
	for (int i = 0; i < 8; ++i)
		if ((mask & (1 << i)) > 0)
			++active;
	return active;
}

/* returns the # of bits left to read for the last blockstate */
int read_blockstate(int offset, int mask_size, uint8_t mask, int **current_blockstate, uint64_t nbt) {
	int read = offset;
	if (offset > 0) {
		int mask_offset = mask_size - offset;
		**current_blockstate |= (nbt & (mask >> mask_offset)) << mask_offset;
		++(*current_blockstate);
	}

	offset = 0;
	nbt >>= read;
	while (read + mask_size < 64) {
		**current_blockstate = nbt & mask;
		++(*current_blockstate);
		read += mask_size;
		nbt >>= mask_size;
	}

	**current_blockstate = nbt & mask;
	offset = read + mask_size - 64;
	if (offset == 0)
		++(*current_blockstate);
	return offset;
}

void read_blockstates(int blockstates[4096], int bits_per_block, size_t nbt_len, uint64_t *nbt) {
	uint8_t mask = mask_gen(bits_per_block);
	int offset = 0;
	for (size_t i = 0; i < nbt_len; ++i)
		offset = read_blockstate(offset, bits_per_block, mask, &blockstates, nbt[i]);
}
