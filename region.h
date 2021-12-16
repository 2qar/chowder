#ifndef CHOWDER_REGION_H
#define CHOWDER_REGION_H

#include "chunk.h"
#include "hashmap.h"

struct region {
	int x;
	int z;
	struct chunk *chunks[32][32];
};

void free_region(struct region *);

#endif
