#ifndef CHOWDER_CHUNK_H
#define CHOWDER_CHUNK_H

#include "section.h"

#define BIOMES_LEN 1024

struct chunk {
	int sections_len;
	struct section *sections[17];
	int *biomes;
};

void free_chunk(struct chunk *);

#endif // CHOWDER_CHUNK_H
