#include <math.h>

#include "section.h"

uint8_t bitmask(int size) {
	/* probably not necessary xd */
	if (size > 8)
		return 0xff;

	uint8_t mask = 0;
	for (int i = 0; i < size; ++i)
		mask |= 1 << i;
	return mask;
}

int block_index(int x, int y, int z) {
	return x + (z * 16) + (y * 16 * 16);
}

int read_blockstate_at(const struct section *s, int x, int y, int z) {
	uint64_t mask = bitmask(s->bits_per_block);
	int offset = block_index(x, y, z) * s->bits_per_block;

	int blockstates_idx = (int) floor(offset / 64.0);
	offset %= 64;

	if (offset + s->bits_per_block > 64) {
		mask = bitmask(64 - offset);
	}
	int blockstate = (s->blockstates[blockstates_idx] & (mask << offset)) >> offset;
	if (offset + s->bits_per_block > 64) {
		int bits_left = offset + s->bits_per_block - 64;
		mask = bitmask(bits_left);
		blockstate |= (s->blockstates[blockstates_idx + 1] & mask) << (s->bits_per_block - bits_left);
	}
	return blockstate;
}

void write_blockstate_at(struct section *s, int x, int y, int z, int value) {
	/* TODO: between read_blockstate_at and write_blockstate_at, 
	 *       the first like 6 lines are identical, so maybe
	 *       make a function for that
	 */
	uint64_t mask = bitmask(s->bits_per_block);
	int offset = block_index(x, y, z) * s->bits_per_block;

	int blockstates_idx = (int) floor(offset / 64.0);
	offset %= 64;

	if (offset + s->bits_per_block > 64) {
		mask = bitmask(64 - offset);
	}
	uint64_t *blockstate = &(s->blockstates[blockstates_idx]);
	*blockstate &= (UINT64_MAX - (mask << offset));
	*blockstate |= ((uint64_t) value & mask) << offset;
	if (offset + s->bits_per_block > 64) {
		int bits_left = offset + s->bits_per_block - 64;
		mask = bitmask(bits_left);
		++blockstate;
		*blockstate &= UINT64_MAX - mask;
		*blockstate |= (((uint64_t) value >> (s->bits_per_block - bits_left)) & mask) << (s->bits_per_block - bits_left);
	}
}
