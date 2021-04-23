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

/* returns the length of a string w/ a block's name + all of it's properties
 * and values, like "minecraft:water;level=5"
 */
size_t block_name_and_properties_length(struct nbt *block) {
	struct nbt *name = nbt_get(block, TAG_String, "Name");
	/* FIXME: relying on asserts here is kinda lame */
	assert(name != NULL);
	size_t name_len = strlen(name->data.string);

	struct nbt *properties = nbt_get(block, TAG_Compound, "Properties");
	if (properties != NULL) {
		struct node *l = properties->data.children;
		while (!list_empty(l)) {
			struct nbt *property = list_item(l);
			name_len += strlen(property->name);
			name_len += strlen(property->data.string);
			name_len += 2;
			l = list_next(l);
		}
	}

	return name_len;
}

int block_property_cmp(const void *p1, const void *p2) {
	const struct nbt *prop1 = *(struct nbt **) p1;
	const struct nbt *prop2 = *(struct nbt **) p2;

	return strcoll(prop1->name, prop2->name);
}

size_t sorted_properties(struct nbt *properties_nbt, struct nbt ***properties) {
	size_t properties_len = list_len(properties_nbt->data.children);
	*properties = malloc(sizeof(struct nbt *) * properties_len);

	struct node *l = properties_nbt->data.children;
	size_t i = 0;
	while (!list_empty(l)) {
		(*properties)[i] = list_item(l);
		++i;
		l = list_next(l);
	}

	if (properties_len > 1) {
		qsort(*properties, properties_len, sizeof(struct nbt *), block_property_cmp);
	}
	return properties_len;
}

int palette_entry_to_block_id(struct nbt *block) {
	size_t name_len = block_name_and_properties_length(block);
	char *name = malloc(sizeof(char) * (name_len + 1));

	size_t properties_len = 0;
	struct nbt **properties = NULL;
	struct nbt *properties_nbt = nbt_get(block, TAG_Compound, "Properties");
	if (properties_nbt != NULL) {
		properties_len = sorted_properties(properties_nbt, &properties);
	}

	struct nbt *block_name = nbt_get(block, TAG_String, "Name");
	assert(block_name != NULL);
	name[0] = '\0';
	strcat(name, block_name->data.string);
	for (size_t i = 0; i < properties_len; ++i) {
		/* FIXME: name is """guaranteed""" to be big enough,
		 *        but this is still icky
		 */
		strcat(name, ";");
		strcat(name, properties[i]->name);
		strcat(name, "=");
		strcat(name, properties[i]->data.string);
	}

	int id = block_id(name);
	free(name);
	free(properties);
	return id;
}

void build_palette(struct section *s, struct nbt_list *palette) {
	s->palette_len = list_len(palette->head);
	s->bits_per_block = (int) ceil(log2(s->palette_len));
	if (s->bits_per_block < 4)
		s->bits_per_block = 4;
	else if (s->bits_per_block > 8)
		s->bits_per_block = GLOBAL_BITS_PER_BLOCK;

	s->palette = malloc(sizeof(int) * s->palette_len);
	struct node *l = palette->head;
	int i = 0;
	while (!list_empty(l)) {
		s->palette[i] = palette_entry_to_block_id(list_item(l));
		++i;
		l = list_next(l);
	}
}

struct chunk *parse_chunk(size_t chunk_data_len, uint8_t *chunk_data) {
	struct chunk *c = malloc(sizeof(struct chunk));

	struct nbt *n = nbt_unpack(chunk_data_len, chunk_data);
	if (n == NULL) {
		fprintf(stderr, "fuck\n");
		return NULL;
	}
	struct nbt *level = nbt_get(n, TAG_Compound, "Level");
	assert(level != NULL);
	struct nbt *sections = nbt_get(level, TAG_List, "Sections");
	if (sections == NULL) {
		fprintf(stderr, "couldn't find sections, aborting\n");
		return NULL;
	}

	struct node *l = sections->data.list->head;
	c->sections_len = 0;
	while (!list_empty(l)) {
		struct nbt *s_nbt = list_item(l);
		struct section *s = calloc(1, sizeof(struct section));

		struct nbt *y_index = nbt_get(s_nbt, TAG_Byte, "Y");
		if (y_index != NULL) {
			s->y = y_index->data.t_byte;
		}

		struct nbt *palette = nbt_get(s_nbt, TAG_List, "Palette");
		s->bits_per_block = -1;
		s->palette_len = -1;
		if (palette != NULL) {
			build_palette(s, palette->data.list);
		}

		struct nbt *blockstates = nbt_get(s_nbt, TAG_Long_Array, "BlockStates");
		if (blockstates != NULL) {
			s->blockstates = (uint64_t *) blockstates->data.array->data.longs;
			blockstates->data.array->data.longs = NULL;
		}

		c->sections[c->sections_len] = s;
		++(c->sections_len);
		l = list_next(l);
	}

	c->biomes = NULL;
	struct nbt *biomes = nbt_get(level, TAG_Int_Array, "Biomes");
	if (biomes != NULL) {
		assert(biomes->data.array->len == BIOMES_LEN);
		c->biomes = biomes->data.array->data.ints;
		biomes->data.array->data.ints = NULL;
	}

	nbt_free(n);
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
	free(c->biomes);
	free(c);
}

/* FIXME: misleading name, frees region data but not the struct itself. idk */
void free_region(struct region *r) {
	for (int z = 0; z < 32; ++z)
		for (int x = 0; x < 32; ++x)
			if (r->chunks[z][x] != NULL)
				free_chunk(r->chunks[z][x]);
}
