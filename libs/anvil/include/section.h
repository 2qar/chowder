#ifndef CHOWDER_SECTION_H
#define CHOWDER_SECTION_H

#include <stdint.h>

#define TOTAL_BLOCKSTATES 4096
#define BLOCKSTATES_LEN(bits_per_block)                                        \
	(TOTAL_BLOCKSTATES * bits_per_block / 64)
// NOTE: this is probably a dumb assumption to make, but it works for 1.15.2
#define SECTION_LIGHT_LEN 2048

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
	uint8_t *sky_light;
	uint8_t *block_light;
};

int read_blockstate_at(const struct section *s, int x, int y, int z);
void write_blockstate_at(struct section *s, int x, int y, int z, int value);

void free_section(struct section *);

#endif // CHOWDER_SECTION_H
