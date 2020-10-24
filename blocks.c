#include <assert.h>
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
	if (f == NULL) {
		char err[256];
		snprintf(err, 256, "error opening '%s'", blocks_json_path);
		perror(err);
		return NULL;
	}
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

int jstrncmp(char *s, char *blocks_json, jsmntok_t *t) {
	return strncmp(s, blocks_json + t->start, toklen(t));
}

/* assumes that the block IDs in blocks.json are sorted */
int max_block_id(char *blocks_json, int tokens_len, jsmntok_t *tokens) {
	int i = tokens_len - 1;
	int max_id_index = -1;
	while (i > 0 && max_id_index < 0) {
		if (tokens[i].type == JSMN_STRING && 
				jstrncmp("id", blocks_json, &(tokens[i])) == 0) {
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

struct json {
	char *src;
	int tokens_len;
	jsmntok_t *tokens;
};

int jseek(struct json *j, char *s, int from) {
	int index = -1;

	for (int i = from; i < j->tokens_len && index == -1; ++i) {
		if (j->tokens[i].type == JSMN_STRING &&
				jstrncmp(s, j->src, &(j->tokens[i])) == 0) {
			index = i;
		}
	}

	return index;
}

int parse_block_states(struct json *j, char *block_name, int state_index, int *count) {
	char prop_name[256] = {0};
	snprintf(prop_name, 256, "%s", block_name);
	int state_end = j->tokens[state_index].end;
	int i = state_index;

	int properties_index = jseek(j, "properties", state_index);
	if (properties_index != -1 && j->tokens[properties_index].start < state_end) {
		i = properties_index;

		++i;
		int prop_end = j->tokens[i].end;
		assert(j->tokens[i].type == JSMN_OBJECT);
		++i;
		while (j->tokens[i].start < prop_end) {
			/* add property name */
			strcat(prop_name, ";");
			strncat(prop_name, j->src + j->tokens[i].start, toklen(&(j->tokens[i])));
			++i;
			/* add property value */
			strcat(prop_name, "=");
			strncat(prop_name, j->src + j->tokens[i].start, toklen(&(j->tokens[i])));
			++i;
		}
	}

	int id_index = jseek(j, "id", state_index);
	if (id_index != -1 && j->tokens[id_index].start < state_end) {
		i = id_index + 1;
		assert(j->tokens[i].type == JSMN_PRIMITIVE);

		char id_str[16];
		snprintf(id_str, 16, "%.*s", toklen(&(j->tokens[i])), j->src + j->tokens[i].start);
		int id = atoi(id_str);

		ENTRY e;
		e.key = malloc(sizeof(char) * strlen(prop_name)+1);
		snprintf(e.key, strlen(prop_name)+1, "%s", prop_name);
		e.data = malloc(sizeof(int));
		memcpy(e.data, (void *) &id, sizeof(int));

		if (hsearch(e, ENTER) == NULL) {
			fprintf(stderr, "error adding to table\n");
		} else {
			++(*count);
		}
	}

	i = state_index;
	while (i < j->tokens_len && j->tokens[i].start < state_end)
		++i;

	//if (j->tokens[i].type == JSMN_UNDEFINED)
	//	i = j->tokens_len;

	return i;
}

/* parse each of the block IDs into a hashtable */
int parse_blocks_json(struct json *j) {
	int count = 0;
	int i = 1;
	int name_len = 128;
	char name[128] = {0};
	while (i < j->tokens_len && j->tokens[i].type == JSMN_STRING) {
		snprintf(name, name_len, "%.*s", toklen(&(j->tokens[i])), j->src + j->tokens[i].start);
		++i;

		assert(j->tokens[i].type == JSMN_OBJECT);
		int block_token_end = j->tokens[i].end;

		int states_index = jseek(j, "states", i);
		if (states_index != -1) {
			i = states_index + 1;
			assert(j->tokens[i].type == JSMN_ARRAY);
			int states_index = i;
			int states_end = j->tokens[states_index].end;
			++i;
			assert(j->tokens[i].type == JSMN_OBJECT);
			while (i < j->tokens_len && j->tokens[i].start < states_end)
				i = parse_block_states(j, name, i, &count);
		}

		while (i < j->tokens_len && j->tokens[i].start < block_token_end)
			++i;
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

	struct json j = {0};
	j.src = blocks_json;
	j.tokens_len = tokens;
	j.tokens = t;
	int block_ids = max_block_id(blocks_json, tokens, t) + 1;
	/* hcreate(3) said 25% extra space helps w/ performance sooo */
	hcreate(block_ids * (block_ids / 4));
	int parsed = parse_blocks_json(&j);
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

int block_id(char *name) {
	int id = 0;

	ENTRY e;
	e.key = name;
	ENTRY *found = hsearch(e, FIND);
	if (found != NULL)
		id = *((int *) found->data);

	return id;
}
