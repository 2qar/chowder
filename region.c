#include <endian.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>

#include "region.h"
#include "blocks.h"
#include "nbt.h"

#define JSMN_HEADER
#include "include/jsmn/jsmn.h"

#define COMPRESSION_TYPE_ZLIB 2

#define GLOBAL_BITS_PER_BLOCK 14

ssize_t read_chunk(FILE *f, int x, int z, size_t *chunk_buf_len, Bytef **chunk) {
	/* read the first chunk offset in the region file */
	fseek(f, 4 * ((x % 32) + (z % 32) * 32), SEEK_SET);
	int offset = 0;
	for (int i = 2; i >= 0; --i)
		offset += fgetc(f) << (8 * i);
	offset *= 4096;

	/* read sectors len */
	uLongf chunk_len = fgetc(f) * 4096;
	if (chunk_len == 0 && offset == 0)
		return 0;

	/* read the first chunk's header */
	fseek(f, offset, SEEK_SET);
	uLongf compressed_len = 0;
	for (int i = 3; i >= 0; --i)
		compressed_len += fgetc(f) << (8 * i);

	/* check that compression type matches */
	assert(fgetc(f) == COMPRESSION_TYPE_ZLIB);
	
	/* read the first chunk's compressed data into a buffer */
	Bytef *compressed_chunk = malloc(sizeof(Bytef) * chunk_len);
	int n = fread(compressed_chunk, (size_t) sizeof(Bytef), compressed_len, f);
	if (n == 0) {
		perror("fread");
		return -1;
	}

	/* uncompress the chunk data */
	/* FIXME: this 100 here is just a random number
	 *        that happened to be big enough to hold the uncompressed data */
	uLong uncompressed_len = sizeof(Bytef) * chunk_len * 100;
	if (uncompressed_len > *chunk_buf_len) {
		*chunk_buf_len = uncompressed_len;
		*chunk = realloc(*chunk, *chunk_buf_len);
	}
	int result = uncompress(*chunk, &uncompressed_len, compressed_chunk, compressed_len);
	if (result != Z_OK) {
		fprintf(stderr, "error uncompressing data: %d\n", result);
		return -1;
	}
	free(compressed_chunk);

	return uncompressed_len;
}

void read_blockstates(struct section *s, struct nbt *nbt_data, int blockstates_index) {
	nbt_data->_index = blockstates_index;
	nbt_skip_tag_name(nbt_data);

	size_t blockstates_len = nbt_read_int(nbt_data);
	uint64_t *blockstates = malloc(sizeof(uint64_t) * blockstates_len);
	memcpy(blockstates, nbt_data->data + nbt_data->_index, s->bits_per_block * TOTAL_BLOCKSTATES / 8);
	for (size_t i = 0; i < blockstates_len; ++i)
		blockstates[i] = be64toh(blockstates[i]);

	s->blockstates = blockstates;
}

int strcoll_cmp(const void *p1, const void *p2) {
	const char *s1 = *((const char **) p1);
	const char *s2 = *((const char **) p2);

	return strcoll(s1, s2);
}

char *parse_block_properties(struct nbt *n, int properties_index) {
	n->_index = properties_index;
	nbt_skip_tag_name(n);

	int properties_len = 0;
	char *properties[8] = {0};
	size_t combined_len = 0;
	while (n->data[n->_index] != TAG_End) {
		int prop_start = n->_index;

		char *property = malloc(sizeof(char) * 64);
		int r = nbt_read_tag_name(n, 64, property);
		strcat(property, "=");
		++r;
		n->_index = prop_start;
		r += nbt_read_string(n, 64 - r, property + r);

		properties[properties_len] = property;
		combined_len += r;
		++properties_len;
	}

	char *s = "";
	if (properties_len > 0) {
		qsort(properties, properties_len, sizeof(char *), strcoll_cmp);

		combined_len += properties_len;
		char *combined = malloc(sizeof(char) * combined_len);
		int written = 0;
		for (int i = 0; i < properties_len; ++i) {
			char *start = combined + written;
			size_t remaining = combined_len - written;
			written += snprintf(start, remaining, ";%s", properties[i]);
		}

		for (int i = 0; i < properties_len; ++i)
			free(properties[i]);
		s = combined;
	}

	return s;
}

int parse_palette_entry(struct nbt *n) {
	size_t name_len = 128;
	char *name = malloc(sizeof(char) * name_len);

	int block_start = n->_index;
	int block_end = nbt_compound_seek_end(n);
	n->_index = block_start;

	int name_index = nbt_compound_seek_tag(n, TAG_String, "Name");
	n->_index = name_index;
	nbt_read_string(n, name_len, name);

	n->_index = block_start;
	int properties_index = nbt_compound_seek_tag(n, TAG_Compound, "Properties");
	if (properties_index != -1) {
		char *properties = parse_block_properties(n, properties_index);
		strncat(name, properties, name_len - strlen(name) - 1);
		free(properties);
	}
	n->_index = block_end;

	int id = block_id(name);
	free(name);
	return id;
}

void parse_palette(struct section *s, struct nbt *n, int palette_index) {
	n->_index = palette_index;
	s->palette_len = nbt_list_len(n);
	s->bits_per_block = (int) ceil(log2(s->palette_len));
	if (s->bits_per_block < 4)
		s->bits_per_block = 4;
	else if (s->bits_per_block > 8)
		s->bits_per_block = GLOBAL_BITS_PER_BLOCK;

	s->palette = malloc(sizeof(int) * s->palette_len);
	for (int i = 0; i < s->palette_len; ++i)
		s->palette[i] = parse_palette_entry(n);
}

struct chunk *parse_chunk(Bytef *chunk_data) {
	struct chunk *c = malloc(sizeof(struct chunk));

	struct nbt nbt_data = {0};
	nbt_data.data = chunk_data;
	int section_index = nbt_tag_seek(&nbt_data, TAG_List, "Sections");
	if (section_index == -1) {
		fprintf(stderr, "couldn't find sections, aborting\n");
		return NULL;
	}
	c->sections_len = nbt_list_len(&nbt_data);

	for (int i = 0; i < c->sections_len; ++i) {
		struct section *s = calloc(1, sizeof(struct section));
		int section_start = nbt_data._index;
		int section_end = nbt_compound_seek_end(&nbt_data);
		nbt_data._index = section_start;

		/* read this section's Y index */
		int y_index = nbt_compound_seek_tag(&nbt_data, TAG_Byte, "Y");
		if (y_index != -1) {
			nbt_data._index = y_index;
			s->y = nbt_read_byte(&nbt_data);
		}
		nbt_data._index = section_start;

		/* read palette */
		int palette_index = nbt_compound_seek_tag(&nbt_data, TAG_List, "Palette");
		s->bits_per_block = -1;
		s->palette_len = -1;
		if (palette_index != -1)
			parse_palette(s, &nbt_data, palette_index);

		/* read blockstates */
		nbt_data._index = section_start;
		int blockstates_index = nbt_compound_seek_tag(&nbt_data, TAG_Long_Array, "BlockStates");
		if (blockstates_index != -1 && s->bits_per_block != -1) {
			s->blockstates = malloc(sizeof(int) * TOTAL_BLOCKSTATES);
			read_blockstates(s, &nbt_data, blockstates_index);
		}

		nbt_data._index = section_end;
		c->sections[i] = s;
	}

	return c;
}

void free_section(struct section *s) {
	free(s->palette);
	free(s->blockstates);
	free(s);
}

void free_chunk(struct chunk *c) {
	for (int i = 0; i < c->sections_len; ++i)
		free_section(c->sections[i]);
	free(c);
}

/* FIXME: misleading name, frees region data but not the struct itself. idk */
void free_region(struct region *r) {
	for (int y = 0; y < 32; ++y)
		for (int x = 0; x < 32; ++x)
			if (r->chunks[y][x] != NULL)
				free_chunk(r->chunks[y][x]);
}
