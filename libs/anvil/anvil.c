#include "anvil.h"

#include "nbt.h"
#include "region.h"

#include <assert.h>
#include <endian.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define COMPRESSION_TYPE_ZLIB 2
#define GLOBAL_BITS_PER_BLOCK 14

enum anvil_err anvil_read_chunk(FILE *f, int x, int z, size_t *chunk_buf_len,
				Bytef **chunk, size_t *out_len)
{
	fseek(f, 4 * ((x & 31) + (z & 31) * 32), SEEK_SET);
	int chunk_offset = 0;
	for (int i = 2; i >= 0; --i)
		chunk_offset += fgetc(f) << (8 * i);
	chunk_offset *= 4096;

	uLongf sectors = fgetc(f);
	if (sectors == 0 && chunk_offset == 0)
		return ANVIL_CHUNK_MISSING;

	fseek(f, chunk_offset, SEEK_SET);
	uLongf compressed_len = 0;
	for (int i = 3; i >= 0; --i)
		compressed_len += fgetc(f) << (8 * i);
	compressed_len -= 1;
	assert(fgetc(f) == COMPRESSION_TYPE_ZLIB);
	Bytef *compressed_chunk = malloc(sizeof(Bytef) * compressed_len);
	int n =
	    fread(compressed_chunk, (size_t) sizeof(Bytef), compressed_len, f);
	if (n == 0) {
		perror("fread");
		return ANVIL_READ_ERROR;
	}

	z_stream stream = { 0 };
	stream.next_in = compressed_chunk;
	stream.avail_in = compressed_len;
	int z_err = inflateInit(&stream);
	if (z_err != Z_OK) {
		fprintf(stderr, "zlib error: %d\n", z_err);
		return ANVIL_ZLIB_ERROR;
	}
	if (sectors * 4096 > *chunk_buf_len) {
		*chunk_buf_len = sectors * 4096;
		*chunk = reallocarray(*chunk, *chunk_buf_len, sizeof(Bytef));
	}
	stream.next_out = *chunk;
	stream.avail_out = *chunk_buf_len;
	while ((z_err = inflate(&stream, Z_NO_FLUSH)) == Z_BUF_ERROR
	       || (z_err == Z_OK && stream.avail_out == 0)) {
		*chunk_buf_len += 4096;
		stream.avail_out = 4096;
		*chunk = reallocarray(*chunk, *chunk_buf_len, sizeof(Bytef));
		stream.next_out = *chunk + stream.total_out;
	}
	inflateEnd(&stream);
	free(compressed_chunk);
	if (z_err == Z_STREAM_END) {
		*out_len = (size_t) stream.total_out;
		return ANVIL_OK;
	} else {
		fprintf(stderr, "zlib inflate error: %d\n", z_err);
		return ANVIL_ZLIB_ERROR;
	}
}

/* returns the length of a string w/ a block's name + all of it's properties
 * and values, like "minecraft:water;level=5"
 */
static size_t block_name_and_properties_length(struct nbt *block)
{
	struct nbt *name = nbt_get(block, TAG_String, "Name");
	/* FIXME: relying on asserts here is kinda lame */
	assert(name != NULL);
	size_t name_len = strlen(name->data.string);

	struct nbt *properties = nbt_get(block, TAG_Compound, "Properties");
	if (properties != NULL) {
		struct list *l = properties->data.children;
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

static int block_property_cmp(const void *p1, const void *p2)
{
	const struct nbt *prop1 = *(struct nbt **) p1;
	const struct nbt *prop2 = *(struct nbt **) p2;

	return strcoll(prop1->name, prop2->name);
}

static size_t sorted_properties(struct nbt *properties_nbt,
				struct nbt ***properties)
{
	size_t properties_len = list_len(properties_nbt->data.children);
	*properties = malloc(sizeof(struct nbt *) * properties_len);

	struct list *l = properties_nbt->data.children;
	size_t i = 0;
	while (!list_empty(l)) {
		(*properties)[i] = list_item(l);
		++i;
		l = list_next(l);
	}

	if (properties_len > 1) {
		qsort(*properties, properties_len, sizeof(struct nbt *),
		      block_property_cmp);
	}
	return properties_len;
}

static int palette_entry_to_block_id(struct hashmap *block_table,
				     struct nbt *block)
{
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

	int *id = hashmap_get(block_table, name);
	free(name);
	free(properties);
	if (id == NULL) {
		fprintf(stderr, "no block id for block '%s'\n", name);
		return 0;
	} else {
		return *id;
	}
}

static void build_palette(struct hashmap *block_table, struct section *s,
			  struct nbt_list *palette)
{
	s->palette_len = list_len(palette->head);
	s->bits_per_block = (int) ceil(log2(s->palette_len));
	if (s->bits_per_block < 4)
		s->bits_per_block = 4;
	else if (s->bits_per_block > 8)
		s->bits_per_block = GLOBAL_BITS_PER_BLOCK;

	s->palette = malloc(sizeof(int) * s->palette_len);
	struct list *l = palette->head;
	int i = 0;
	while (!list_empty(l)) {
		s->palette[i] =
		    palette_entry_to_block_id(block_table, list_item(l));
		++i;
		l = list_next(l);
	}
}

enum anvil_err anvil_parse_chunk(struct hashmap *block_table,
				 size_t chunk_data_len, uint8_t *chunk_data,
				 struct chunk **out)
{
	struct nbt *n;
	struct nbt *data_version;
	size_t n_len = nbt_unpack(chunk_data_len, chunk_data, &n);
	if (n_len == 0) {
		return ANVIL_BAD_NBT;
	} else if ((data_version = nbt_get(n, TAG_Int, "DataVersion")) == NULL
		   || data_version->data.t_int != ANVIL_DATA_VERSION) {
		nbt_free(n);
		return ANVIL_BAD_DATA_VERSION;
	}
	struct nbt *level = nbt_get(n, TAG_Compound, "Level");
	assert(level != NULL);
	struct nbt *sections = nbt_get(level, TAG_List, "Sections");
	if (sections == NULL) {
		fprintf(stderr, "couldn't find sections, aborting\n");
		return ANVIL_BAD_CHUNK;
	}

	struct list *l = sections->data.list->head;
	struct chunk *c = malloc(sizeof(struct chunk));
	c->sections_len = 0;
	// FIXME: remove this
	c->player_count = 0;
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
			build_palette(block_table, s, palette->data.list);
		}

		struct nbt *blockstates =
		    nbt_get(s_nbt, TAG_Long_Array, "BlockStates");
		if (blockstates != NULL) {
			s->blockstates =
			    (uint64_t *) blockstates->data.array->data.longs;
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
	*out = c;
	return ANVIL_OK;
}

enum anvil_err get_chunk(FILE *region_file, struct hashmap *block_table, int x,
			 int z, size_t *chunk_buf_len, Bytef **chunk_buf,
			 struct chunk **out)
{
	size_t chunk_data_len = 0;
	enum anvil_err err = anvil_read_chunk(region_file, x, z, chunk_buf_len,
					      chunk_buf, &chunk_data_len);
	if (err == ANVIL_OK) {
		return anvil_parse_chunk(block_table, chunk_data_len,
					 *chunk_buf, out);
	} else {
		return err;
	}
}

enum anvil_err anvil_get_chunk(FILE *region_file, struct hashmap *block_table,
			       int x, int z, struct chunk **out)
{
	size_t chunk_buf_len = 0;
	Bytef *chunk_buf = NULL;
	enum anvil_err err = get_chunk(region_file, block_table, x, z,
				       &chunk_buf_len, &chunk_buf, out);
	free(chunk_buf);
	return err;
}

enum anvil_err anvil_get_chunks(struct anvil_get_chunks_ctx *ctx,
				struct region *region)
{
	// FIXME: these checks probably won't work properly for negative chunks
	assert(ctx->cx1 / 32 == ctx->cx2 / 32);
	assert(ctx->cz1 / 32 == ctx->cz2 / 32);

	size_t chunk_buf_len = 0;
	Bytef *chunk_buf = NULL;
	struct chunk *chunk = NULL;
	enum anvil_err err = ANVIL_OK;
	int z = ctx->cz1;
	int x = ctx->cx1;
	// FIXME: ANVIL_CHUNK_MISSING shouldn't be an acceptable error
	int missing = 0;
	while (z <= ctx->cz2
	       && (err == ANVIL_OK || err == ANVIL_CHUNK_MISSING)) {
		x = ctx->cx1;
		while (x <= ctx->cx2
		       && (err == ANVIL_OK || err == ANVIL_CHUNK_MISSING)) {
			err = get_chunk(ctx->region_file, ctx->block_table, x,
					z, &chunk_buf_len, &chunk_buf, &chunk);
			if (err == ANVIL_CHUNK_MISSING) {
				++missing;
			} else {
				region_set_chunk(region, x, z, chunk);
			}
			++x;
		}
		++z;
	}
	free(chunk_buf);
	ctx->missing = missing;
	if (err != ANVIL_OK && err != ANVIL_CHUNK_MISSING) {
		ctx->err_x = x - 1;
		ctx->err_z = z - 1;
		return err;
	} else {
		return ANVIL_OK;
	}
}
