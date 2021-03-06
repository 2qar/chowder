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
#include <search.h>

/* FIXME: this include here is hacky, it's only here because
 *        the include in blocks.c doesn't work */
#include "../../include/jsmn/jsmn.h"
#include "../../blocks.h"
#include "../../region.h"

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
int is_region_file(const char *filename);
struct pos *parse_chunk_pos(const char *in);
struct world_pos *parse_world_pos(const char *in);
struct chunk *chunk_at(const char *filename, int x, int z);
void print_section(struct section *, const struct pos *);
void print_sections(struct chunk *, struct pos *);
void print_block_at(struct chunk *, struct world_pos *);

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("usage: cv FILE [COORDS...]\n");
		exit(EXIT_FAILURE);
	}

	if (!is_region_file(argv[1])) {
		fprintf(stderr, "cv: \"%s\" isn't a region file\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	int world_coords = argv[2][0] == 'w';
	void *p;
	if (world_coords) {
		p = parse_world_pos(argv[2]);
		if (p == NULL) {
			fprintf(stderr, "cv: expected \"x,y,z\", got \"%s\"\n", argv[2]);
			exit(EXIT_FAILURE);
		}
	} else {
		p = parse_chunk_pos(argv[2]);
		if (p == NULL) {
			fprintf(stderr, "cv: expected \"x,z\", got \"%s\"\n", argv[2]);
			exit(EXIT_FAILURE);
		}
	}
	
	int failed = create_block_table(BLOCKS_JSON_PATH);
	if (failed) {
		fprintf(stderr, "cv: error creating block table\n");
		exit(EXIT_FAILURE);
	}
	struct chunk *c;
	if (world_coords) {
		struct world_pos *w = p;
		int x = w->x / 16;
		int z = w->z / 16;
		c = chunk_at(argv[1], x, z);
	} else {
		struct pos *pos = p;
		c = chunk_at(argv[1], pos->x, pos->z);
	}

	if (world_coords) {
		print_block_at(c, (struct world_pos *) p);
	} else {
		struct pos *pos = p;
		if (pos->section != -1) {
			if (pos->section < c->sections_len) {
				print_section(c->sections[pos->section], pos);
			} else {
				fprintf(stderr, "cv: %d > chunk sections length\n", pos->section);
				exit(EXIT_FAILURE);
			}
		} else {
			print_sections(c, pos);
		}
	}

	free(p);
	hdestroy();
	exit(EXIT_SUCCESS);
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

struct chunk *chunk_at(const char *filename, int x, int z) {
	FILE *f = fopen(filename, "r");

	Bytef *chunk_buf = NULL;
	size_t chunk_buf_len = 0;
	ssize_t len = read_chunk(f, x, z, &chunk_buf_len, &chunk_buf);
	if (len == -1) {
		fprintf(stderr, "cv: error reading chunk\n");
		/* FIXME: bad, exiting AND printing inside of a function */
		exit(EXIT_FAILURE);
	} else if (len == 0) {
		fprintf(stderr, "cv: no chunk at \"%d,%d\"\n", x, z);
		exit(EXIT_FAILURE);
	}
	struct chunk *c = parse_chunk(len, chunk_buf);
	if (c == NULL) {
		fprintf(stderr, "cv: error parsing chunk\n");
		exit(EXIT_FAILURE);
	}

	fclose(f);
	return c;
}

void print_section(struct section *s, const struct pos *p) {
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

void print_sections(struct chunk *c, struct pos *p) {
	for (int i = 0; i < c->sections_len; ++i) {
		p->section = i;
		print_section(c->sections[i], p);
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
