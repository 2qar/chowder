#ifndef CHOWDER_REGION_H
#define CHOWDER_REGION_H

#include <stdio.h>
#include <stdint.h>

#include <zlib.h>

struct section {
	int8_t y;
	int palette_len;
	/* TODO: make this an array of palette_entry structs,
	 *       holding the ID + a reference count
	 *       so entries can be removed when their ref count hits 0.
	 *       maybe just set the ID to -1 when their ref count hits 0 idk */
	int *palette;
	int bits_per_block;
	int *blockstates;
};

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
size_t network_blockstates(const struct section *, uint64_t **);
void free_chunk(struct chunk *);
void free_region(struct region *);

#endif
