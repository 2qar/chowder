#include "blocks.h"

#include "hashmap.h"
#include "json.h"
#include "list.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BLOCK_NAMES
size_t block_names_len;
char **block_names;

void free_block_names()
{
	free(block_names);
}
#endif

static void update_block_table_size(char *key, void *value, void *data)
{
	(void) key;
	int64_t *size = (int64_t *) data;
	struct json_value *states = json_get(value, "states");
	assert(states != NULL && states->type == JSON_ARRAY);
	struct list *state_list = states->array;
	while (!list_empty(state_list)) {
		struct json_value *state = list_item(state_list);
		if (json_get(state, "properties") != NULL) {
			++(*size);
		}
		if (json_get(state, "default") != NULL) {
			++(*size);
		}
		state_list = list_next(state_list);
	}
}

struct block_property {
	char *name;
	char *value;
};

int block_property_compare(const void *p1, const void *p2)
{
	const struct block_property *b1 = p1;
	const struct block_property *b2 = p2;
	return strcmp(b1->name, b2->name);
}

static void add_block_state_to_table(char name_with_properties[256],
				     struct json_value *id_json,
				     struct hashmap *block_table, bool add_name)
{
#ifndef BLOCK_NAMES
	(void) add_name;
#endif
	int64_t *id = malloc(sizeof(int64_t));
	*id = id_json->integer;
	char *name = strdup(name_with_properties);
	hashmap_add(block_table, name, id);
#ifdef BLOCK_NAMES
	if (add_name) {
		block_names[*id] = name;
	}
#endif
}

static void add_property(char *name, void *value, void *data)
{
	struct json_value *value_json = value;
	struct block_property **property_list = data;
	(**property_list).name = name;
	(**property_list).value = value_json->string;
	++(*property_list);
}

static void append_property_to_name(char name_with_properties[256],
				    char *property_name, char *property_value)
{
	size_t off = strlen(name_with_properties);
	size_t property_str_len =
	    strlen(property_name) + strlen(property_value) + strlen(";=");
	size_t n = snprintf(name_with_properties + off, 256 - off, ";%s=%s",
			    property_name, property_value);
	if (n != property_str_len) {
		fprintf(stderr, "truncated block state \"%s\"\n",
			name_with_properties);
	}
}

static void append_properties_to_name(char name_with_properties[256],
				      struct json_value *properties)
{
	size_t properties_list_len = json_members(properties);
	struct block_property *properties_list =
	    calloc(properties_list_len, sizeof(struct block_property));
	struct block_property *head = properties_list;
	json_apply(properties, add_property, &head);
	qsort(properties_list, properties_list_len,
	      sizeof(struct block_property), block_property_compare);
	for (size_t i = 0; i < properties_list_len; ++i) {
		append_property_to_name(name_with_properties,
					properties_list[i].name,
					properties_list[i].value);
	}
	free(properties_list);
}

static void add_block_state(char name_with_properties[256],
			    struct json_value *state_json,
			    struct hashmap *block_table)
{
	struct json_value *id_json = json_get(state_json, "id");
	bool added_name = false;
	assert(id_json != NULL);
	if (json_get(state_json, "default") != NULL) {
		add_block_state_to_table(name_with_properties, id_json,
					 block_table, !added_name);
		added_name = true;
	}
	struct json_value *properties = json_get(state_json, "properties");
	if (properties != NULL) {
		append_properties_to_name(name_with_properties, properties);
		add_block_state_to_table(name_with_properties, id_json,
					 block_table, !added_name);
	}
}

static void add_block_states(char *name, void *value, void *data)
{
	struct hashmap *block_table = data;
	char name_with_properties[256] = { 0 };
	struct json_value *states_json = json_get(value, "states");
	assert(states_json != NULL && states_json->type == JSON_ARRAY);
	struct list *states = states_json->array;
	while (!list_empty(states)) {
		snprintf(name_with_properties, 256, "%s", name);
		add_block_state(name_with_properties, list_item(states),
				block_table);
		states = list_next(states);
	}
}

struct hashmap *create_block_table(char *block_json_path)
{
	FILE *json_file = fopen(block_json_path, "r");
	if (json_file == NULL) {
		perror("fopen");
		return NULL;
	}
	struct json_value *root;
	char *json_str;
	struct json_err_ctx json_err =
	    json_parse_file(json_file, &root, &json_str);
	fclose(json_file);
	if (json_err.type != JSON_OK) {
		fprintf(stderr, "parsing blocks.json failed, err=%d\n",
			json_err.type);
		return NULL;
	}

	int64_t block_table_size = 0;
	json_apply(root, update_block_table_size, &block_table_size);
#ifdef BLOCK_NAMES
	block_names_len = block_table_size;
	block_names = calloc(block_names_len, sizeof(char *));
#endif

	struct hashmap *block_table = hashmap_new(block_table_size);
	json_apply(root, add_block_states, (void *) block_table);
	json_free(root);
	free(json_str);
	return block_table;
}
