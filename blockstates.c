#include <stdlib.h>
#include <endian.h>

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

void read_blockstates(int blockstates[BLOCKSTATES_LEN], int bits_per_block, size_t nbt_len, uint64_t *nbt) {
	uint8_t mask = mask_gen(bits_per_block);
	int offset = 0;
	for (size_t i = 0; i < nbt_len; ++i)
		offset = read_blockstate(offset, bits_per_block, mask, &blockstates, nbt[i]);
}

int write_blockstate(int offset, int bits_per_block, int **current_blockstate, uint64_t *out) {
	int written = offset;
	if (offset > 0) {
		*out |= **current_blockstate >> (bits_per_block - offset);
		++(*current_blockstate);
	}

	while (written + bits_per_block < 64) {
		*out |= **current_blockstate << written;
		++(*current_blockstate);
		written += bits_per_block;
	}

	offset = written + bits_per_block - 64;
	if (offset > 0) {
		int readable = bits_per_block - offset;
		*out |= (**current_blockstate & mask_gen(readable)) << written;
	}

	return offset;
}

size_t write_blockstates(int blockstates[BLOCKSTATES_LEN], int bits_per_block, uint64_t **out) {
	size_t out_len = BLOCKSTATES_LEN * bits_per_block / sizeof(uint64_t);
	*out = malloc(sizeof(uint64_t) * out_len);

	/* write blockstates in host order */
	int offset = 0;
	for (size_t i = 0; i < out_len; ++i) {
		offset = write_blockstate(offset, bits_per_block, &blockstates, *out + i);
	}

	/* switch them to network order */
	for (size_t i = 0; i < out_len; ++i) {
		(*out)[i] = htobe64((*out)[i]);
	}

	return out_len;
}
