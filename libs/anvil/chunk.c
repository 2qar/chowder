#include "chunk.h"

#include <stdlib.h>

void free_chunk(struct chunk *c)
{
	for (int i = 0; i < c->sections_len; ++i)
		free_section(c->sections[i]);
	free(c->biomes);
	free(c);
}
