#include "section.h"

#include <math.h>
#include <stdlib.h>

uint8_t bitmask(int size)
{
	/* probably not necessary xd */
	if (size > 8)
		return 0xff;

	return (1 << size) - 1;
}

struct block_pos {
	uint64_t mask;
	uint64_t offset;
	int start_long;
	int end_long;
};

struct block_pos block_pos(const struct section *s, int x, int y, int z)
{
	x %= 16;
	y %= 16;
	z %= 16;

	struct block_pos p;
	p.mask = bitmask(s->bits_per_block);

	int block_index = x + ((y * 16) + z) * 16;
	p.offset = (block_index * s->bits_per_block) % 64;
	p.start_long = (block_index * s->bits_per_block) / 64;
	p.end_long = ((block_index + 1) * s->bits_per_block) / 64;
	if (64 - p.offset == (uint64_t) s->bits_per_block) {
		p.end_long = p.start_long;
	}

	return p;
}

/* Based on the deserialization implementation by #mcdevs
 * https://wiki.vg/Chunk_Format#Deserializing
 */
int read_blockstate_at(const struct section *s, int x, int y, int z)
{
	struct block_pos p = block_pos(s, x, y, z);
	int palette_index = s->blockstates[p.start_long] >> p.offset;
	if (p.start_long != p.end_long) {
		int end_offset = 64 - p.offset;
		palette_index |= (s->blockstates[p.end_long] << end_offset);
	}
	return palette_index & p.mask;
}

void write_blockstate_at(struct section *s, int x, int y, int z, int value)
{
	struct block_pos p = block_pos(s, x, y, z);
	uint64_t v = value & p.mask;
	s->blockstates[p.start_long] |= (v << p.offset);
	if (p.start_long != p.end_long) {
		int end_offset = 64 - p.offset;
		s->blockstates[p.end_long] |= v >> end_offset;
	}
}

void free_section(struct section *s)
{
	free(s->palette);
	free(s->blockstates);
	free(s->sky_light);
	free(s->block_light);
	free(s);
}
