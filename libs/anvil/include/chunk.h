#ifndef CHOWDER_CHUNK_H
#define CHOWDER_CHUNK_H

#include "section.h"

#define BIOMES_LEN	   1024
#define CHUNK_SECTIONS_LEN 18

struct chunk {
	int sections_len;
	struct section *sections[CHUNK_SECTIONS_LEN];
	int *biomes;

	/* FIXME: this doesn't belong in anvil. this chunk struct should be
	 *        'anvil_chunk', and a seperate chunk struct in the main src/
	 *        should have this reference counter. */
	// # of players that can see this chunk
	int player_count;
};

void free_chunk(struct chunk *);

#endif // CHOWDER_CHUNK_H
