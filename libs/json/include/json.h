#ifndef CHOWDER_JSON_H
#define CHOWDER_JSON_H
#include "hashmap.h"
#include "list.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum json_type {
	JSON_OBJECT = 1,
	JSON_ARRAY,
	JSON_STRING,
	// TODO: support arbitrary precision
	JSON_INTEGER,
	JSON_FRACTION,
	JSON_TRUE,
	JSON_FALSE,
	JSON_NULL,

	// don't use this one.
	JSON_UNINITIALIZED = 0,
};

struct json_value {
	enum json_type type;
	union {
		struct json *object;
		struct list *array;
		char *string;
		int64_t integer;
		double fraction;
	};
};

struct json;

enum json_err {
	JSON_OK,
	JSON_INVALID_STRING,
	JSON_INVALID_NUMBER,
	JSON_MISSING_CLOSING,
	JSON_TRAILING_COMMA,
	JSON_UNEXPECTED_CHAR,
	JSON_UNEXPECTED_EOF,
	JSON_EXPECTED_TRUTHY,
	JSON_EXPECTED_VALUE,
	JSON_FREAD,
};

struct json_err_ctx {
	enum json_err type;
	size_t index;
	union {
		char expected;
	};
};

struct json_value *json_new();
/* json_parse() modifies the given string, so don't try to print or operate on
 * it unless you want to be disappointed */
struct json_err_ctx json_parse(char *json_string, struct json_value **out);
struct json_err_ctx json_parse_file(FILE *json_file, struct json_value **out,
				    char **out_json_string);
void json_set(struct json_value *root_object, char *key,
	      struct json_value *value);
struct json_value *json_get(struct json_value *root_object, char *key);
size_t json_members(struct json_value *object);
bool json_equal(struct json_value *, struct json_value *);
void json_apply(struct json_value *root_object, hm_apply_func, void *data);
void json_free(struct json_value *);

#endif // CHOWDER_JSON_H
