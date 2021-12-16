/* cv reads chunk data from a .mca file and prints information about the chunks
 * in that region.
 *
 * Ex. "cv r.0.0.mca 0,0" will print every section in the first chunk in r.0.0.mca.
 *
 * Calling it with an extra number, eg. "cv r.0.0.mca 0,0,5", will print the
 * section at that index, if the number you gave is a valid index.
 *
 * Prefixing the "x,y,z" coordinates with a 'w' will print the block in
 * the world at those global coordinates.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "anvil.h"
#include "blocks.h"
#include "hashmap.h"

#define BLOCKS_JSON_PATH "../../gamedata/blocks.json"

struct pos {
	int x;
	int z;
	int section;
};

struct world_pos {
	int x;
	int y;
	int z;
};
void usage();
int is_region_file(const char *filename);
struct pos *parse_chunk_pos(const char *in);
struct world_pos *parse_world_pos(const char *in);
struct chunk *chunk_at(const char *filename, struct hashmap *block_table, int x, int z);

typedef void (*print_section_func)(const struct section *, const struct pos *);
void pretty_print_section(const struct section *, const struct pos *);
void raw_print_section(const struct section *, const struct pos *);
void print_sections(const struct chunk *, struct pos *, print_section_func);
typedef void (*print_chunk_func)(const struct chunk *, struct pos *);
void pretty_print_chunk(const struct chunk *, struct pos *);
void raw_print_chunk(const struct chunk *, struct pos *);
void print_block_at(struct chunk *, struct world_pos *);

int main(int argc, char **argv) {
	bool pretty_print = true;
	char optchar;
	while ((optchar = getopt(argc, argv, "hR")) != -1) {
		switch (optchar) {
			case 'R':
				pretty_print = false;
				break;
			case 'h':
				usage();
				exit(EXIT_SUCCESS);
			case '?':
				usage();
				exit(EXIT_FAILURE);
		}
	}
	if (optind > argc - 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	const size_t region_file_index = optind;
	if (!is_region_file(argv[region_file_index])) {
		fprintf(stderr, "cv: \"%s\" isn't a region file\n", argv[region_file_index]);
		exit(EXIT_FAILURE);
	}

	const size_t coords_index = optind + 1;
	int world_coords = argv[coords_index][0] == 'w';
	void *p;
	if (world_coords) {
		p = parse_world_pos(argv[coords_index]);
		if (p == NULL) {
			fprintf(stderr, "cv: expected \"x,y,z\", got \"%s\"\n", argv[coords_index]);
			exit(EXIT_FAILURE);
		}
	} else {
		p = parse_chunk_pos(argv[coords_index]);
		if (p == NULL) {
			fprintf(stderr, "cv: expected \"x,z\", got \"%s\"\n", argv[coords_index]);
			exit(EXIT_FAILURE);
		}
	}
	
	struct hashmap *block_table = create_block_table(BLOCKS_JSON_PATH);
	if (block_table == NULL) {
		fprintf(stderr, "cv: error creating block table\n");
		exit(EXIT_FAILURE);
	}
	struct chunk *c;
	if (world_coords) {
		struct world_pos *w = p;
		int x = w->x / 16;
		int z = w->z / 16;
		c = chunk_at(argv[region_file_index], block_table, x, z);
	} else {
		struct pos *pos = p;
		c = chunk_at(argv[region_file_index], block_table, pos->x, pos->z);
	}
	if (c == NULL) {
		exit(EXIT_FAILURE);
	}

	if (world_coords) {
		print_block_at(c, (struct world_pos *) p);
	} else {
		struct pos *pos = p;
		if (pos->section >= c->sections_len) {
			fprintf(stderr, "cv: %d > chunk sections length\n", pos->section);
			exit(EXIT_FAILURE);
		} else if (pos->section != -1) {
			print_section_func print_section = pretty_print ?
				pretty_print_section : raw_print_section;
			print_section(c->sections[pos->section], pos);
		} else {
			print_chunk_func print_chunk = pretty_print ?
				pretty_print_chunk : raw_print_chunk;
			print_chunk(c, pos);
		}
	}

	hashmap_free(block_table, true, free);
	free(p);
	free_chunk(c);
	free_block_names();
	exit(EXIT_SUCCESS);
}

void usage() {
	printf("usage:\n"
		"  cv [options] region_file x,z          Pretty-print a chunk\n"
		"  cv [options] region_file x,section,z  Pretty-print one section\n"
		"  cv [options] region_file wx,y,z       Print the block in this region at the global x,y,z coords\n"
		"\noptions:\n"
		"  -R: instead of pretty-printing sections, print the following line-seperated values:\n"
		"        section Y-index\n"
		"        palette length\n"
		"        palette entries, one per line\n"
		"        bits per block\n"
		"        blockstates, one per line, compacted into unsigned 64-bit integers (https://wiki.vg/Chunk_Format#Compacted_data_array)\n"
		"      when printing a chunk, the following additional info is printed:\n"
		"        number of sections\n"
		"        sections, formatted like above\n"
		"        biomes; 1024 line-seperated ints\n"
		"  -h: print this info and exit\n"
	    );
}

int is_region_file(const char *filename) {
	char *dot = strrchr(filename, '.');
	return (dot != NULL && strcmp(dot + 1, "mca") == 0);
}


struct pos *parse_chunk_pos(const char *in) {
	struct pos *p = malloc(sizeof(struct pos));
	p->section = -1;

	int parsed = sscanf(in, "%d,%d,%d", &(p->x), &(p->z), &(p->section));
	if (parsed < 2) {
		p = NULL;
	}
	return p;
}

struct world_pos *parse_world_pos(const char *in) {
	struct world_pos *p = malloc(sizeof(struct world_pos));

	int parsed = sscanf(in, "w%d,%d,%d", &(p->x), &(p->y), (&p->z));
	if (parsed != 3) {
		p = NULL;
	}
	return p;
}

struct chunk *chunk_at(const char *filename, struct hashmap *block_table, int x, int z) {
	FILE *f = fopen(filename, "r");
	struct chunk *c = NULL;
	enum anvil_err err = anvil_get_chunk(f, block_table, x, z, &c);
	fclose(f);
	switch (err) {
		case ANVIL_OK:
			break;
		case ANVIL_CHUNK_MISSING:
			fprintf(stderr, "cv: no chunk at (%d,%d)\n", x, z);
			break;
		case ANVIL_READ_ERROR:
		case ANVIL_ZLIB_ERROR:
			fprintf(stderr, "cv: error reading chunk\n");
			break;
		case ANVIL_BAD_NBT:
		case ANVIL_BAD_CHUNK:
			fprintf(stderr, "cv: error parsing chunk\n");
			break;
	}
	return c;
}

void pretty_print_section(const struct section *s, const struct pos *p) {
	printf("(%d, %d, %d)\n", p->x, p->z, p->section);
	printf("    y = %d\n", s->y);
	if (s->palette_len > 0) {
		printf("    palette: %d entries [\n", s->palette_len);
		for (int i = 0; i < s->palette_len; ++i) {
			printf("        %2d = %s,\n", i, block_names[s->palette[i]]);
		}
		printf("    ]\n");

		printf("    bits_per_block = %d\n", s->bits_per_block);
		printf("    blockstates length: %d\n", BLOCKSTATES_LEN(s->bits_per_block));
	}
}


void raw_print_section(const struct section *s, const struct pos *p) {
	(void) p;

	printf("%d\n", s->y);
	printf("%d\n", s->palette_len);
	for (int i = 0; i < s->palette_len; ++i) {
		printf("%d\n", s->palette[i]);
	}
	printf("%d\n", s->bits_per_block);
	for (int i = 0; i < BLOCKSTATES_LEN(s->bits_per_block); ++i) {
		printf("%lu\n", s->blockstates[i]);
	}
}

void print_sections(const struct chunk *c, struct pos *p, print_section_func print_section) {
	for (int i = 0; i < c->sections_len; ++i) {
		p->section = i;
		print_section(c->sections[i], p);
	}
}

void pretty_print_chunk(const struct chunk *c, struct pos *p) {
	print_sections(c, p, pretty_print_section);
}

void raw_print_chunk(const struct chunk *c, struct pos *p) {
	printf("%d\n", c->sections_len);
	print_sections(c, p, raw_print_section);
	for (size_t i = 0; i < BIOMES_LEN; ++i) {
		printf("%d\n", c->biomes[i]);
	}
}

void print_block_at(struct chunk *c, struct world_pos *w) {
	int i = (w->y / 16) + 1;
	if (i < 0 || i >= c->sections_len) {
		fprintf(stderr, "cv: invalid y coordinate %d\n", w->y);
		exit(EXIT_FAILURE);
	}
	struct section *s = c->sections[i];
	i = read_blockstate_at(s, w->x, w->y, w->z);
	printf("section coords: (%d,%d,%d)\n", w->x / 16, s->y, w->z / 16);
	printf("global coords: (%d,%d,%d) = %s\n", w->x, w->y, w->z, block_names[s->palette[i]]);
	printf("in-chunk coords: (%d,%d,%d)\n", w->x % 16, w->y % 16, w->z % 16);
}
