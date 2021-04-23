#ifndef CHOWDER_REGION_H
#define CHOWDER_REGION_H

#include <stdio.h>

#include <zlib.h>

#include "section.h"
#include "include/hashmap.h"

#define BIOMES_LEN 1024

struct chunk {
	int sections_len;
	struct section *sections[16];
	int *biomes;
};

struct region {
	int x;
	int z;
	struct chunk *chunks[32][32];
};

ssize_t read_chunk(FILE *f, int x, int y, size_t *chunk_buf_len, Bytef **chunk);
struct chunk *parse_chunk(struct hashmap *block_table, size_t len, uint8_t *chunk_data);
void free_chunk(struct chunk *);
void free_region(struct region *);

#endif
