#include <stdint.h>
#include <stddef.h>

#define BLOCKSTATES_LEN 4096

void read_blockstates(int blockstates[BLOCKSTATES_LEN], int bits_per_block, size_t nbt_len, uint64_t *nbt);
size_t write_blockstates(int blockstates[BLOCKSTATES_LEN], int bits_per_block, uint64_t **out);
