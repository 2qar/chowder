/* cv reads chunk data from a .mca file and prints information about the chunks
 * in that region.
 *
 * Ex. "cv r.0.0.mca 0,0" will print every section in the first chunk in r.0.0.mca.
 *
 * Calling it with an extra number, eg. "cv r.0.0.mca 0,0,5", will print the
 * section at that index, if the number you gave is a valid index.
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
int is_region_file(const char *filename);
struct pos *parse_chunk_pos(const char *in);
struct chunk *chunk_at(const char *filename, int x, int z);
void print_section(struct section *, const struct pos *);
void print_sections(struct chunk *, struct pos *);

int main(int argc, char **argv) {
	if (argc <= 1) {
		printf("usage: cv FILE [COORDS...]\n");
		exit(EXIT_FAILURE);
	}

	if (!is_region_file(argv[1])) {
		fprintf(stderr, "cv: \"%s\" isn't a region file\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	struct pos *p = parse_chunk_pos(argv[2]);
	if (p == NULL) {
		fprintf(stderr, "cv: expected \"x,z\", got \"%s\"\n", argv[2]);
		exit(EXIT_FAILURE);
	}
	
	int failed = create_block_table(BLOCKS_JSON_PATH);
	if (failed) {
		fprintf(stderr, "cv: error creating block table\n");
		exit(EXIT_FAILURE);
	}
	struct chunk *c = chunk_at(argv[1], p->x, p->z);

	if (p->section != -1) {
		if (p->section < c->sections_len) {
			print_section(c->sections[p->section], p);
		} else {
			fprintf(stderr, "cv: %d > chunk sections length\n", p->section);
			exit(EXIT_FAILURE);
		}
	} else {
		print_sections(c, p);
	}

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
		return NULL;
	} else {
		return p;
	}
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
	struct chunk *c = parse_chunk(chunk_buf);
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
