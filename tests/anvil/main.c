#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "anvil.h"
#include "blocks.h"
#include "chunk.h"
#include "hashmap.h"

void read_cv_section(FILE *out_file, struct section *section)
{
	// FIXME: you should check these scanf return values, stupid
	fscanf(out_file, "%hhd\n", &section->y);
	fscanf(out_file, "%d\n", &section->palette_len);
	if (section->palette_len > 0) {
		section->palette = calloc(section->palette_len, sizeof(int));
	}
	for (int i = 0; i < section->palette_len; ++i) {
		fscanf(out_file, "%d\n", &section->palette[i]);
	}
	fscanf(out_file, "%d\n", &section->bits_per_block);
	size_t blockstates_len = 0;
	if (section->bits_per_block != -1) {
		blockstates_len = BLOCKSTATES_LEN(section->bits_per_block);
		section->blockstates = calloc(blockstates_len, sizeof(uint64_t));
	}
	for (size_t i = 0; i < blockstates_len; ++i) {
		fscanf(out_file, "%lu\n", &section->blockstates[i]);
	}
}

struct chunk *read_cv_chunk(FILE *out_file)
{
	struct chunk *chunk = calloc(1, sizeof(struct chunk));
	fscanf(out_file, "%d\n", &chunk->sections_len);
	for (int i = 0; i < chunk->sections_len; ++i) {
		chunk->sections[i] = malloc(sizeof(struct chunk));
		read_cv_section(out_file, chunk->sections[i]);
	}
	chunk->biomes = calloc(BIOMES_LEN, sizeof(int));
	for (size_t i = 0; i < BIOMES_LEN; ++i) {
		fscanf(out_file, "%d\n", &chunk->biomes[i]);
	}
	return chunk;
}

bool array_equal(const void *a1, const void *a2, size_t bytes)
{
	return (a1 == NULL && a2 == NULL)
		|| (a1 != NULL && a2 != NULL && !memcmp(a1, a2, bytes));
}

bool section_equal(const struct section *s1, const struct section *s2)
{
	return s1->y == s2->y
		&& s1->palette_len == s2->palette_len
		&& (s1->palette_len == -1 || array_equal(s1->palette, s2->palette, sizeof(int) * s1->palette_len))
		&& s1->bits_per_block == s2->bits_per_block
		&& (s1->bits_per_block == -1 || array_equal(s1->blockstates, s2->blockstates, sizeof(uint64_t) * BLOCKSTATES_LEN(s1->bits_per_block)));
}

bool sections_equal(size_t sections_len, struct section *s1[16], struct section *s2[16])
{
	size_t i = 0;
	while (i < sections_len && section_equal(s1[i], s2[i])) {
		++i;
	}
	return i == sections_len;
}

bool chunks_equal(struct chunk *c1, struct chunk *c2)
{
	return c1->sections_len == c2->sections_len
		&& sections_equal(c1->sections_len, c1->sections, c2->sections)
		&& array_equal(c1->biomes, c2->biomes, sizeof(int) * BIOMES_LEN);
}

int main()
{
	FILE *region_file = fopen("../r.0.0.mca", "r");
	assert(region_file != NULL);
	FILE *cv_out_file = fopen("chunk_0-0.cvout", "r");
	assert(cv_out_file != NULL);
	struct chunk *chunk = read_cv_chunk(cv_out_file);
	assert(chunk != NULL);
	struct hashmap *block_table = create_block_table("../../gamedata/blocks.json");
	struct chunk *anvil_chunk = NULL;
	enum anvil_err err = anvil_get_chunk(region_file, block_table, 0, 0, &anvil_chunk);
	assert(err == ANVIL_OK);
	assert(chunks_equal(anvil_chunk, chunk));

	hashmap_free(block_table, true, free);
	free_chunk(anvil_chunk);
	free_chunk(chunk);
}
