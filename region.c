#include <endian.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>

#include "region.h"
#include "blocks.h"
#include "blockstates.h"
#include "nbt.h"

#define JSMN_HEADER
#include "include/jsmn/jsmn.h"

#define COMPRESSION_TYPE_ZLIB 2

ssize_t read_chunk(FILE *f, int x, int y, size_t *chunk_buf_len, Bytef **chunk) {
	/* read the first chunk offset in the region file */
	fseek(f, 4 * x * y, SEEK_SET);
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

void parse_blockstates(struct section *s, struct nbt *nbt_data, int blockstates_index) {
	nbt_data->_index = blockstates_index;
	nbt_skip_tag_name(nbt_data);
	size_t blockstates_len = nbt_read_int(nbt_data);
	uint64_t *nbt_blockstates = malloc(sizeof(uint64_t) * blockstates_len);
	memcpy(nbt_blockstates, nbt_data->data + nbt_data->_index, s->bits_per_block * 4096 / 8);
	for (size_t i = 0; i < blockstates_len; ++i)
		nbt_blockstates[i] = be64toh(nbt_blockstates[i]);
	read_blockstates(s->blockstates, s->bits_per_block, blockstates_len, nbt_blockstates);
	free(nbt_blockstates);
}

char *parse_block_properties(struct nbt *n, int properties_index) {
	char *properties = malloc(sizeof(char) * 128);
	properties[0] = '\0';

	n->_index = properties_index;
	nbt_skip_tag_name(n);

	/* read + append each property to the block's name */
	while (n->data[n->_index] != TAG_End) {
		int prop_start = n->_index;

		char prop_name[64] = {0};
		nbt_read_tag_name(n, 64, prop_name);
		strcat(properties, ";");

		strncat(properties, prop_name, 64);
		strcat(properties, "=");
		n->_index = prop_start;

		char prop_value[64] = {0};
		nbt_read_string(n, 64, prop_value);
		strncat(properties, prop_value, 64);
	}

	return properties;
}

int parse_palette_entry(struct nbt *n) {
	size_t name_len = 128;
	char *name = malloc(sizeof(char) * name_len);

	int block_start = n->_index;
	nbt_compound_seek_end(n);
	int block_end = n->_index;
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
	printf("parsing %d sections\n", c->sections_len);

	for (int i = 0; i < c->sections_len; ++i) {
		struct section *s = calloc(1, sizeof(struct section));
		int section_start = nbt_data._index;

		/* read this section's Y index */
		int y_index = nbt_compound_seek_tag(&nbt_data, TAG_Byte, "Y");
		if (y_index != -1) {
			/* XXX: For some reason, the first section of every chunk is
			 *      y = -1 instead of y = 0 ???? */
			/* TODO: use a proper read_byte function */
			s->y = (int8_t) nbt_data.data[y_index + 4];
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
			s->blockstates = malloc(sizeof(int) * BLOCKSTATES_LEN);
			parse_blockstates(s, &nbt_data, blockstates_index);
		}

		nbt_data._index = section_start;
		nbt_compound_seek_end(&nbt_data);
		c->sections[i] = s;
	}

	return c;
}

size_t network_blockstates(const struct section *s, uint64_t **out) {
	return write_blockstates(s->blockstates, s->bits_per_block, out);
}

void free_section(struct section *s) {
	free(s->palette);
	free(s->blockstates);
}

void free_chunk(struct chunk *c) {
	for (int i = 0; i < c->sections_len; ++i)
		free_section(c->sections[i]);
}

/* FIXME: misleading name, frees region data but not the struct itself. idk */
void free_region(struct region *r) {
	for (int y = 0; y < 32; ++y)
		for (int x = 0; x < 32; ++x)
			if (r->chunks[y][x] != NULL)
				free_chunk(r->chunks[y][x]);
}
