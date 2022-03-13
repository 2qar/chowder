#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lexer.h"
#include "parser.h"
#include "gen.h"

size_t file_bytes(const char *filename, char **out)
{
	FILE *f = fopen(filename, "r");
	if (!f) {
		perror("fopen");
		return 0;
	}
		
	fseek(f, 0L, SEEK_END);
	size_t f_len = ftell(f);
	rewind(f);
	char *f_bytes = calloc(f_len + 1, sizeof(char));
	size_t n_read = fread(f_bytes, 1, f_len, f);
	fclose(f);
	if (n_read != f_len) {
		free(f_bytes);
		fprintf(stderr, "read %zd, expected %ld\n", n_read, f_len);
		return 0;
	}
	*out = f_bytes;
	return n_read;
}

char *packet_name(const char *packet_filename)
{
	char *slash = strrchr(packet_filename, '/');
	if (slash)
		packet_filename = slash + 1;

	char *name = NULL;
	char *dot = strchr(packet_filename, '.');
	size_t name_len;
	if (dot) {
		name_len = dot - packet_filename + 1;
	} else {
		name_len = strlen(packet_filename) + 1;
	}
	name = calloc(name_len, sizeof(char));
	snprintf(name, name_len, "%s", packet_filename);
	return name;
}

static char *make_path(const char *prefix, char *filename, const char *extension)
{
	if (prefix == NULL || prefix[0] == '\0') {
		prefix = ".";
	}
	size_t path_len = strlen(prefix) + strlen(filename) + strlen(extension) + 2;
	char *path = calloc(path_len, sizeof(char));
	snprintf(path, path_len, "%s/%s%s", prefix, filename, extension);
	return path;
}

static void usage()
{
	fprintf(stderr, "usage: pc [-o output_dir] PACKET_FILE\n");
}

int main(int argc, char *argv[])
{
	char *output_dir = NULL;
	int opt;
	struct stat output_dir_stat;
	while ((opt = getopt(argc, argv, "o:")) != -1) {
		switch (opt) {
			case 'o':
				output_dir = optarg;
				break;
			default:
				usage();
				return 1;
		}
	}
	if (optind >= argc) {
		usage();
		return 1;
	} else if (output_dir != NULL) {
		if (stat(output_dir, &output_dir_stat) < 0) {
			perror("stat");
			return 1;
		} else if (!S_ISDIR(output_dir_stat.st_mode)) {
			fprintf(stderr, "\"%s\" isn't a directory\n", output_dir);
			return 1;
		}
	}
	char *packet_filename = argv[optind];

	char *bytes = NULL;
	size_t bytes_len = file_bytes(packet_filename, &bytes);
	if (!bytes || !bytes_len)
		return 1;

	char *name = packet_name(packet_filename);
	unsigned id;
	if (!sscanf(bytes, "id = 0x%x\n", &id)) {
		fprintf(stderr, "malformed ID\n");
		return 1;
	}

	struct token *tokens = lexer_parse(bytes);

	struct token *t = tokens;
	while (!token_equals(t, "\n")) {
		t = t->next;
	}
	while (token_equals(t, "\n")) {
		t = t->next;
	}
	struct field *head = calloc(1, sizeof(struct field));
	struct field *f = head;
	while (t && t->line != 0) {
		t = parse_field(t, f);
		if (f->next)
			f = f->next;
	}
	if (!t)
		return 1;
	free_tokens(tokens);

	create_parent_links(head);
	if (resolve_field_name_refs(head))
		return 1;

	char *source_path = make_path(output_dir, name, ".c");
	char *header_path = make_path(output_dir, name, ".h");

	FILE *header_file = freopen(header_path, "w", stdout);
	if (header_file == NULL) {
		perror("freopen");
		return 1;
	}
	put_id(name, id);
	put_includes();
	generate_structs(name, head);
	put_function_signatures(name);
	fclose(header_file);
	free(header_path);

	FILE *source_file = freopen(source_path, "w", stdout);
	if (source_file == NULL) {
		perror("freopen");
		return 1;
	}
	generate_source(id, name, head);
	fclose(source_file);
	free(source_path);

	free_fields(head);
	free(bytes);
	free(name);
	return 0;
}
