#include "json.h"
#include "list.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void put_spaces(size_t n, FILE *stream)
{
	for (size_t i = 0; i < n; ++i) {
		fputc(' ', stream);
	}
}

// FIXME: this won't work with the new wacky mangled strings produced by parsing
void print_error_ctx(const char *json_str, struct json_err_ctx *error)
{
	size_t column = 0;
	size_t i = error->index;
	while (i > 0 && json_str[i] != '\n') {
		--i;
		++column;
	}
	if (json_str[i] == '\n') {
		++i;
		--column;
	}
	size_t bad_line_start = i;
	size_t line = 1;
	while (i > 0) {
		if (json_str[i] == '\n') {
			++line;
		}
		--i;
	}
	int prefix_len = fprintf(stderr, "%zu,%zu | ", line, column);
	i = bad_line_start;
	while (json_str[i] != '\n' && json_str[i] != '\0') {
		fputc(json_str[i], stderr);
		++i;
	}
	fputc('\n', stderr);
	put_spaces(error->index - bad_line_start + prefix_len, stderr);
	fprintf(stderr, "^\n");
	put_spaces(error->index - bad_line_start + prefix_len, stderr);
	fprintf(stderr, "| ");
	switch (error->type) {
	case JSON_UNEXPECTED_CHAR:
		fprintf(stderr, "expected a '%c'\n", error->expected);
		break;
	default:
		fprintf(stderr, "this is bad i guess\n");
		break;
	}
}

static void json_expect_equal(const char *json_str, struct json_value *value)
{
	char *buf = strdup(json_str);
	struct json_value *parsed;
	struct json_err_ctx json_err = json_parse(buf, &parsed);
	if (json_err.type != JSON_OK) {
		print_error_ctx(buf, &json_err);
		exit(EXIT_FAILURE);
	} else {
		assert(json_equal(parsed, value));
		json_free(parsed);
		free(buf);
		json_free(value);
	}
}

static struct json_value *json_value(enum json_type type)
{
	struct json_value *value = malloc(sizeof(struct json_value));
	value->type = type;
	return value;
}

struct member {
	char *name;
	struct json_value *value;
};

static struct json_value *json_object(struct member *members)
{
	struct json_value *value = json_new();
	while ((*members).name != NULL) {
		json_set(value, (*members).name, (*members).value);
		++members;
	}
	return value;
}

static struct json_value *json_string(char *s)
{
	struct json_value *value = json_value(JSON_STRING);
	value->string = s;
	return value;
}

static struct json_value *json_integer(int64_t i)
{
	struct json_value *value = json_value(JSON_INTEGER);
	value->integer = i;
	return value;
}

static struct json_value *json_fraction(double d)
{
	struct json_value *value = json_value(JSON_FRACTION);
	value->fraction = d;
	return value;
}

static struct json_value *json_true()
{
	return json_value(JSON_TRUE);
}

static struct json_value *json_false()
{
	return json_value(JSON_FALSE);
}

static struct json_value *json_null()
{
	return json_value(JSON_NULL);
}

static void json_expect_error(const char *json_str,
			      struct json_err_ctx expected_error)
{
	char *buf = strdup(json_str);
	struct json_err_ctx error = json_parse(buf, NULL);
	assert(error.type == expected_error.type);
	assert(error.index == expected_error.index);
	switch (error.type) {
	case JSON_UNEXPECTED_CHAR:
		assert(error.expected == expected_error.expected);
		break;
	default:
		break;
	}
	free(buf);
}

static struct json_err_ctx json_error(enum json_err type, size_t index)
{
	return (struct json_err_ctx) {
		.type = type,
		.index = index,
	};
}

static struct json_err_ctx json_unexpected_char(size_t index, char expected)
{
	struct json_err_ctx err = json_error(JSON_UNEXPECTED_CHAR, index);
	err.expected = expected;
	return err;
}

void test_simple()
{
	struct json_value *root;
	char json_str[] = "{ \"thomas\": \r\n\t-1234, \"ogdog\": null, "
			  "\"atcat\": 12e-2, \"obj\": { \"ogdog\": 1 } }";
	struct json_err_ctx error = json_parse(json_str, &root);
	if (error.type != JSON_OK) {
		print_error_ctx(json_str, &error);
		exit(EXIT_FAILURE);
	} else {
		struct member obj_members[] = {
			{ "ogdog", json_integer(1) },
			{ 0 },
		};
		struct member members[] = {
			{ "thomas", json_integer(-1234) },
			{ "ogdog", json_null() },
			{ "atcat", json_fraction(0.12) },
			{ "obj", json_object(obj_members) },
			{ 0 },
		};
		struct json_value *expected = json_object(members);
		assert(json_equal(root, expected));
		json_free(root);
		json_free(expected);
	}
}

void test_huge()
{
	FILE *json_file = fopen("../../gamedata/blocks.json", "r");
	struct json_value *root;
	char *json_str;
	struct json_err_ctx error =
	    json_parse_file(json_file, &root, &json_str);
	if (error.type != JSON_OK) {
		print_error_ctx(json_str, &error);
		exit(EXIT_FAILURE);
	} else {
		assert(json_members(root) == 680);
		struct json_value *andesite =
		    json_get(root, "minecraft:andesite");
		assert(andesite != NULL);
		assert(andesite->type == JSON_OBJECT);
		struct json_value *states = json_get(andesite, "states");
		assert(states != NULL);
		assert(states->type == JSON_ARRAY);
		assert(!list_empty(states->array));
		struct json_value *default_state = list_item(states->array);
		assert(default_state != NULL);
		assert(default_state->type == JSON_OBJECT);
		struct json_value *id = json_get(default_state, "id");
		assert(id != NULL);
		assert(id->type == JSON_INTEGER);
		assert(id->integer == 6);
		struct json_value *def = json_get(default_state, "default");
		assert(def != NULL);
		assert(def->type == JSON_TRUE);
		fclose(json_file);
		json_free(root);
		free(json_str);
	}
}

int main()
{
	json_expect_error("", json_error(JSON_EXPECTED_VALUE, 0));
	json_expect_equal("\"thomas\"", json_string("thomas"));
	json_expect_equal("\"esca\\be\"", json_string("esca\be"));
	// FIXME: this should probably point to the offending control character
	json_expect_error("\"esca\be\"", json_error(JSON_INVALID_STRING, 0));
	json_expect_error("\"e", json_error(JSON_MISSING_CLOSING, 0));
	json_expect_equal("-123", json_integer(-123));
	json_expect_error("-", json_error(JSON_INVALID_NUMBER, 0));
	json_expect_equal("+1.5e1", json_fraction(15.0));
	json_expect_error("+1.", json_error(JSON_INVALID_NUMBER, 0));
	json_expect_error("+1.5e-", json_error(JSON_INVALID_NUMBER, 0));
	json_expect_error("+1.5e1.", json_error(JSON_INVALID_NUMBER, 0));
	json_expect_error("+1.5e1.1", json_error(JSON_INVALID_NUMBER, 0));
	json_expect_equal("true", json_true());
	json_expect_equal("false", json_false());
	json_expect_equal("null", json_null());
	json_expect_error("nullp", json_error(JSON_EXPECTED_TRUTHY, 0));
	json_expect_error("\"e\": 1", json_unexpected_char(3, '\0'));
	// FIXME: maybe this should point to the character after the closing
	// quote?
	//        the current behavior of pointing to the first non-whitespace
	//        char is kinda confusing
	json_expect_error("{ \"e\" 1 }", json_unexpected_char(6, ':'));
	json_expect_error("{ \"e\":  }", json_error(JSON_EXPECTED_VALUE, 8));
	json_expect_error("{ \"e\": 1 ", json_error(JSON_UNEXPECTED_EOF, 9));
	json_expect_error("{ \"e\": 1, ", json_error(JSON_UNEXPECTED_EOF, 10));
	json_expect_error("{ \"e\": 1, }", json_error(JSON_TRAILING_COMMA, 8));
	json_expect_error("{ \"e\": 1 \"a\": 4 }",
			  json_unexpected_char(9, ','));
	json_expect_error("[ 1 2 ]", json_unexpected_char(4, ','));
	json_expect_error("[ 1, 2 ", json_error(JSON_UNEXPECTED_EOF, 7));
	json_expect_error("[ 1, 2, ]", json_error(JSON_TRAILING_COMMA, 6));
	json_expect_error("[ 1, 2, ", json_error(JSON_UNEXPECTED_EOF, 8));
	test_simple();
	test_huge();
}
