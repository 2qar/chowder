#ifndef CHOWDER_REGION_H
#define CHOWDER_REGION_H

#include <stdio.h>

#include <zlib.h>

#include "section.h"

struct chunk {
	int sections_len;
	struct section *sections[16];
};

struct region {
	int x;
	int y;
	struct chunk *chunks[32][32];
};

ssize_t read_chunk(FILE *f, int x, int y, size_t *chunk_buf_len, Bytef **chunk);
struct chunk *parse_chunk(Bytef *chunk_data);
void free_chunk(struct chunk *);
void free_region(struct region *);

#endif
