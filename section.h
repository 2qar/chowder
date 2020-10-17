/* Defines a struct for storing chunk section information,
 * and functions for manipulating each section's blockstates array.
 */
#include <stdint.h>

#define TOTAL_BLOCKSTATES 4096
#define BLOCKSTATES_LEN(bits_per_block) (TOTAL_BLOCKSTATES * bits_per_block / 64)

struct section {
	int8_t y;
	int palette_len;
	/* TODO: make this an array of palette_entry structs,
	 *       holding the ID + a reference count
	 *       so entries can be removed when their ref count hits 0.
	 *       maybe just set the ID to -1 when their ref count hits 0 idk */
	int *palette;
	int bits_per_block;
	uint64_t *blockstates;
};

int read_blockstate_at(const struct section *s, int x, int y, int z);
void write_blockstate_at(struct section *s, int x, int y, int z, int value);
