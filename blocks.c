#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>

#include "blocks.h"

#define JSMN_HEADER
#include "include/jsmn/jsmn.h"

/* FIXME: this just happens to work and is also a little too many */
#define TOKENS 800000

char *read_blocks_json(char *blocks_json_path) {
	FILE *f = fopen(blocks_json_path, "r");
	fseek(f, 0L, SEEK_END);
	size_t blocks_json_len = ftell(f);
	rewind(f);
	char *blocks_json = malloc(sizeof(char) * (blocks_json_len + 1));
	size_t n = fread(blocks_json, sizeof(char), blocks_json_len, f);
	if (n < blocks_json_len) {
		perror("error reading blocks file");
		fprintf(stderr, "read %ld, expected %ld\n", n, blocks_json_len);
		free(blocks_json);
		return NULL;
	}
	blocks_json[blocks_json_len] = '\0';
	return blocks_json;
}

int toklen(jsmntok_t *t) {
	return t->end - t->start;
}

int jstrncmp(char *s, char *blocks_json, jsmntok_t *tokens, int i) {
	return strncmp(s, blocks_json + tokens[i].start, toklen(&tokens[i]));
}

/* assumes that the block IDs in blocks.json are sorted */
int max_block_id(char *blocks_json, int tokens_len, jsmntok_t *tokens) {
	int i = tokens_len - 1;
	int max_id_index = -1;
	while (i > 0 && max_id_index < 0) {
		if (tokens[i].type == JSMN_STRING && 
				jstrncmp("id", blocks_json, tokens, i) == 0) {
			max_id_index = i + 1;
		}
		--i;
	}

	int max_id = -1;
	if (max_id_index != -1) {
		char id_str[16] = {0};
		snprintf(id_str, 16, "%.*s", toklen(&tokens[max_id_index]), blocks_json + tokens[max_id_index].start);
		max_id = atoi(id_str);
	}
	return max_id;
}

/* parse each of the block IDs into a hashtable */
int parse_blocks_json(char *blocks_json, int tokens_len, jsmntok_t *tokens) {
	int count = 0;
	int i = 1;
	int name_len = 128;
	char name[128] = {0};
	char prop_name[256] = {0};
	while (i < tokens_len && tokens[i].type == JSMN_STRING) {
		snprintf(name, name_len, "%.*s", toklen(&tokens[i]), blocks_json + tokens[i].start);

		/* read length of block object and skip to first token in it */
		++i;
		int block_token_end = tokens[i].end;

		/* iterate over tokens until we're at the next block */
		while (i < tokens_len && tokens[i].start < block_token_end) {
			++i;
			if (tokens[i].type == JSMN_STRING && 
					jstrncmp("states", blocks_json, tokens, i) == 0) {
				snprintf(prop_name, 256, "%s", name);
				++i;
				int states_end = tokens[i].end;
				/* skip to (maybe) properties */
				++i;
				while (i < tokens_len && tokens[i].start < states_end) {
					if (tokens[i].type == JSMN_STRING) {
						/* FIXME: if this state is the default state (the first one),
						 *        ignore the properties and their values and just
						 *        use the block's name as the key.
						 *        or, insert an extra key for the default state that's
						 *        just the block's name */
						if (jstrncmp("properties", blocks_json, tokens, i) == 0) {
							snprintf(prop_name, 256, "%s", name);
							++i;
							int prop_end = tokens[i].end;
							++i;
							while (tokens[i].start < prop_end) {
								/* FIXME: 7 levels of indentation? 
								 * there has to be a better way 
								 */

								/* add property name */
								strcat(prop_name, ";");
								strncat(prop_name, blocks_json + tokens[i].start, toklen(&tokens[i]));
								++i;
								/* add property value */
								strcat(prop_name, "=");
								strncat(prop_name, blocks_json + tokens[i].start, toklen(&tokens[i]));
								++i;
							}
							--i;
						} else if (jstrncmp("id", blocks_json, tokens, i) == 0) {
							++i;
							char id_str[16];
							snprintf(id_str, 16, "%.*s", toklen(&tokens[i]), blocks_json + tokens[i].start);
							int id = atoi(id_str);

							ENTRY e;
							e.key = malloc(sizeof(char) * strlen(prop_name)+1);
							snprintf(e.key, strlen(prop_name)+1, "%s", prop_name);
							e.data = malloc(sizeof(int));
							memcpy(e.data, (void *) &id, sizeof(int));

							if (hsearch(e, ENTER) == NULL) {
								fprintf(stderr, "error adding to table\n");
							} else {
								++count;
							}
						}
					}
					++i;
				}
			}
		}
	}
	return count;
}

int create_block_table_from_json(char *blocks_json) {
	jsmn_parser p;
	jsmn_init(&p);
	jsmntok_t *t = malloc(sizeof(jsmntok_t) * TOKENS);
	int tokens = jsmn_parse(&p, blocks_json, strlen(blocks_json), t, TOKENS);
	if (tokens < 0) {
		fprintf(stderr, "error parsing blocks.json: %d\n", tokens);
		return 1;
	}
	t = realloc(t, sizeof(jsmntok_t) * tokens);

	int block_ids = max_block_id(blocks_json, tokens, t) + 1;
	/* hcreate(3) said 25% extra space helps w/ performance sooo */
	hcreate(block_ids * (block_ids / 4));
	int parsed = parse_blocks_json(blocks_json, tokens, t);
	if (parsed != block_ids) {
		fprintf(stderr, "too few blocks parsed; parsed (%d) != block_ids (%d)\n", parsed, block_ids);
		return 1;
	}

	free(t);
	return 0;
}

int create_block_table(char *block_json_path) {
	char *blocks_json = read_blocks_json(block_json_path);
	if (blocks_json == NULL)
		return 1;

	int failed = create_block_table_from_json(blocks_json);
	if (failed)
		return 1;

	free(blocks_json);
	return 0;
}
