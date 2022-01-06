#include "json.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json {
	struct hashmap *values;
};

struct json_value *json_new()
{
	struct json_value *object = malloc(sizeof(struct json_value));
	object->type = JSON_OBJECT;
	object->object = malloc(sizeof(struct json));
	object->object->values = hashmap_new(1);
	return object;
}

static bool is_whitespace(char c)
{
	return c == ' '
		|| c == '\n'
		|| c == '\r'
		|| c == '\t';
}

static size_t skip_whitespace(const char *s, size_t start)
{
	size_t i = start;
	while (s[i] != '\0' && is_whitespace(s[i])) {
		++i;
	}
	return i;
}

static bool is_valid_escape(char c)
{
	return c == '"'
		|| c == '\\'
		|| c == '/'
		|| c == 'b'
		|| c == 'f'
		|| c == 'n'
		|| c == 'r'
		|| c == 't';
		// FIXME: handle unicode 'u'
}

static char escape(char c)
{
	static char escaped[] = {
		['"'] = '"',
		['\\'] = '\\',
		['/'] = '/',
		['b'] = '\b',
		['f'] = '\f',
		['n'] = '\n',
		['r'] = '\r',
		['t'] = '\t'
	};
	return escaped[(size_t) c];
}

static size_t parse_string(char *s, size_t open_quote, struct json_err_ctx *error,
		char **out)
{
	size_t i = open_quote + 1;
	size_t string_idx = i;
	bool valid_string = true;
	while (s[i] != '\0' && s[i] != '"' && valid_string) {
		if (s[i] == '\\') {
			++i;
			valid_string = is_valid_escape(s[i]);
			if (valid_string) {
				s[string_idx] = escape(s[i]);
			}
		} else {
			valid_string = !iscntrl(s[i]);
			s[string_idx] = s[i];
		}
		++i;
		++string_idx;
	}
	if (!valid_string) {
		error->type = JSON_INVALID_STRING;
		error->index = open_quote;
		return 0;
	} else if (s[i] == '\0') {
		error->type = JSON_MISSING_CLOSING;
		error->index = open_quote;
		return 0;
	} else {
		s[string_idx] = '\0';
		*out = s + open_quote + 1;
		return i + 1;
	}
}

static size_t parse_value(char *, size_t, struct json_err_ctx *, struct json_value *value);

static size_t parse_member(char *s, size_t open_quote, struct json_err_ctx *error,
		struct json *json)
{
	if (s[open_quote] != '"') {
		error->type = JSON_EXPECTED_VALUE;
		error->index = open_quote;
		return 0;
	}
	char *name = NULL;
	size_t name_end = parse_string(s, open_quote, error, &name);
	if (name_end == 0) {
		return 0;
	}
	size_t colon_idx = skip_whitespace(s, name_end);
	if (s[colon_idx] != ':') {
		error->type = JSON_UNEXPECTED_CHAR;
		error->index = colon_idx;
		error->expected = ':';
		return 0;
	}
	size_t value_start = skip_whitespace(s, colon_idx + 1);
	if (value_start == '\0') {
		error->type = JSON_UNEXPECTED_EOF;
		error->index = value_start;
		return 0;
	}
	struct json_value *json_value = calloc(1, sizeof(struct json_value));
	size_t value_end = parse_value(s, value_start, error, json_value);
	if (value_end == 0) {
		json_free(json_value);
		return 0;
	}
	hashmap_add(json->values, name, json_value);
	return value_end;
}

static size_t skip_to_next_value(const char *s, size_t value_end, struct json_err_ctx *error,
		char value_list_terminator)
{
	size_t i = skip_whitespace(s, value_end);
	if (s[i] == ',') {
		size_t comma_idx = i;
		i = skip_whitespace(s, i + 1);
		if (s[i] == value_list_terminator) {
			error->type = JSON_TRAILING_COMMA;
			error->index = comma_idx;
			return 0;
		} else {
			return i;
		}
	} else if (s[i] == '\0') {
		error->type = JSON_UNEXPECTED_EOF;
		error->index = i;
		return 0;
	} else if (s[i] != value_list_terminator) {
		error->type = JSON_UNEXPECTED_CHAR;
		error->index = i;
		error->expected = ',';
		return 0;
	} else {
		return i;
	}
}

static size_t parse_object(char *s, size_t open_brace, struct json_err_ctx *error,
		struct json_value *json_object)
{
	json_object->type = JSON_OBJECT;
	size_t i = open_brace + 1;
	struct json *json = calloc(1, sizeof(struct json));
	json->values = hashmap_new(5);
	json_object->object = json;
	while (i != 0 && s[i] != '\0' && s[i] != '}') {
		i = parse_member(s, skip_whitespace(s, i), error, json);
		if (i != 0) {
			i = skip_to_next_value(s, i, error, '}');
		}
	}
	if (i == 0) {
		return 0;
	} else if (s[i] == '\0') {
		error->type = JSON_UNEXPECTED_EOF;
		error->index = i;
		return 0;
	} else if (s[i] != '}') {
		error->type = JSON_UNEXPECTED_CHAR;
		error->index = i;
		error->expected = '}';
		return 0;
	} else {
		return i + 1;
	}
}

static size_t parse_array(char *s, size_t open_bracket, struct json_err_ctx *error,
		struct json_value *json_array)
{
	json_array->type = JSON_ARRAY;
	struct list *array = list_new();
	struct list *tail = array;
	json_array->array = array;
	size_t i = skip_whitespace(s, open_bracket + 1);
	while (i != 0 && s[i] != '\0' && s[i] != ']') {
		struct json_value *value = calloc(1, sizeof(struct json_value));
		i = parse_value(s, i, error, value);
		if (i != 0) {
			i = skip_to_next_value(s, i, error, ']');
		}
		list_append(tail, sizeof(struct json_value *), &value);
		tail = tail->next;
	}
	if (i == 0) {
		return 0;
	} else if (s[i] == '\0') {
		error->type = JSON_UNEXPECTED_EOF;
		error->index = i;
		return 0;
	} else if (s[i] != ']') {
		error->type = JSON_UNEXPECTED_CHAR;
		error->index = i;
		error->expected = ']';
		return 0;
	} else {
		return i + 1;
	}
}

static bool is_valid_number(const char *s, size_t *idx, struct json_err_ctx *error,
		enum json_type *type)
{
	size_t num_start = *idx;
	*type = JSON_INTEGER;
	size_t i = *idx;
	if (s[i] == '-' || s[i] == '+') {
		++i;
	}
	if (s[i] == '\0') {
		goto invalid_number;
	}
	while (isdigit(s[i]) && s[i] != '\0') {
		++i;
	}
	if (s[i] == '.') {
		*type = JSON_FRACTION;
		++i;
		if (s[i] == '\0') {
			goto invalid_number;
		}
		while (isdigit(s[i]) && s[i] != '\0') {
			++i;
		}
	}
	if (tolower(s[i]) == 'e') {
		++i;
		if (s[i] == '-') {
			*type = JSON_FRACTION;
			++i;
		} else if (s[i] == '+') {
			++i;
		}
		if (s[i] == '\0') {
			goto invalid_number;
		}
		while (isdigit(s[i]) && s[i] != '\0') {
			++i;
		}
		if (s[i] == '.') {
			goto invalid_number;
		}
	}
	*idx = i;
	return true;
invalid_number:
	error->type = JSON_INVALID_NUMBER;
	error->index = num_start;
	return false;
}

static size_t parse_number(const char *s, size_t num_start, struct json_err_ctx *error,
		struct json_value *value)
{
	size_t num_end = num_start;
	if (!is_valid_number(s, &num_end, error, &value->type)) {
		return 0;
	} else if (value->type == JSON_INTEGER) {
		value->integer = atof(s + num_start);
	} else {
		value->fraction = atof(s + num_start);
	}
	return num_end;
}

static bool is_truthy_follow(char c)
{
	return is_whitespace(c)
		|| c == ','
		|| c == ']'
		|| c == '}'
		|| c == '\0';
}

static size_t parse_truthy(const char *s, size_t i, struct json_err_ctx *error,
		struct json_value *value)
{
	char *truthy_str = NULL;
	switch (s[i]) {
		case 't':
			truthy_str = "true";
			value->type = JSON_TRUE;
			break;
		case 'f':
			truthy_str = "false";
			value->type = JSON_FALSE;
			break;
		case 'n':
			truthy_str = "null";
			value->type = JSON_NULL;
			break;
	}

	size_t truthy_str_len = strlen(truthy_str);
	if (!strncmp(s + i, truthy_str, truthy_str_len)
			&& is_truthy_follow(s[i + truthy_str_len])) {
		return i + truthy_str_len;
	} else {
		error->type = JSON_EXPECTED_TRUTHY;
		error->index = i;
		return 0;
	}
}

static size_t parse_value(char *s, size_t i, struct json_err_ctx *error,
		struct json_value *value)
{
	if (isdigit(s[i])) {
		return parse_number(s, i, error, value);
	} else {
		switch (s[i]) {
		case '+':
		case '-':
			return parse_number(s, i, error, value);
		case '{':
			return parse_object(s, i, error, value);
		case '[':
			return parse_array(s, i, error, value);
		case '"':
			value->type = JSON_STRING;
			return parse_string(s, i, error, &(value->string));
		case 't':
		case 'f':
		case 'n':
			return parse_truthy(s, i, error, value);
		default:
			error->type = JSON_EXPECTED_VALUE;
			error->index = i;
			return 0;
		}
	}
}

// TODO: UTF-8 support. All of the parsing code assumes that each code point is
//       one byte, which works for ASCII strings but won't work if stuff gets
//       wacky. UTF-8 shouldn't be too hard to handle, just make a seperate
//       header/source combo in include/ so the stuff for this parser can be
//       reused in the main project
struct json_err_ctx json_parse(char *json_string, struct json_value **out)
{
	struct json_value *root_value = calloc(1, sizeof(struct json_value));
	struct json_err_ctx error = {0};
	size_t json_end = parse_value(json_string, 0, &error, root_value);
	if (json_end != 0 && json_string[json_end] != '\0') {
		error.type = JSON_UNEXPECTED_CHAR;
		error.index = json_end;
		error.expected = '\0';
		json_free(root_value);
	} else if (json_end == 0) {
		json_free(root_value);
	} else {
		*out = root_value;
	}
	return error;
}

struct json_err_ctx json_parse_file(FILE *json_file, struct json_value **out, char **out_json_string)
{
	fseek(json_file, 0, SEEK_END);
	size_t json_string_len = ftell(json_file);
	char *json_string = calloc(json_string_len + 1, sizeof(char));
	rewind(json_file);
	size_t n = fread(json_string, sizeof(char), json_string_len, json_file);
	if (n != json_string_len) {
		return (struct json_err_ctx) { .type = JSON_FREAD, .index = n };
	} else {
		*out_json_string = json_string;
		return json_parse(json_string, out);
	}
}

void json_set(struct json_value *root_object, char *key, struct json_value *value)
{
	if (root_object->type != JSON_OBJECT) {
		// FIXME: this should probably return an error or something
		return;
	}

	struct json_value *old_value = json_get(root_object, key);
	if (old_value != NULL) {
		json_free(old_value);
	}
	hashmap_add(root_object->object->values, key, value);
}

struct json_value *json_get(struct json_value *root_object, char *key)
{
	struct json_value *value = NULL;
	if (root_object->type == JSON_OBJECT) {
		value = hashmap_get(root_object->object->values, key);
	}
	return value;
}

size_t json_members(struct json_value *value)
{
	if (value->type != JSON_OBJECT) {
		// FIXME: get mad!!!!
		return 0;
	}
	return hashmap_occupied(value->object->values);
}

struct json_obj_comp_ctx {
	bool equal;
	struct json_value *other;
};

static void compare_json_value(char *name, void *val, void *data)
{
	struct json_obj_comp_ctx *ctx = data;
	if (ctx->equal) {
		struct json_value *value = val;
		struct json_value *other_value = json_get(ctx->other, name);
		ctx->equal = other_value != NULL && json_equal(value, other_value);
	}
}

static bool json_object_equal(struct json_value *j1, struct json_value *j2)
{
	if (json_members(j1) != json_members(j2)) {
		return false;
	} else {
		struct json_obj_comp_ctx ctx = {
			.equal = true,
			.other = j2,
		};
		json_apply(j1, compare_json_value, &ctx);
		return ctx.equal;
	}
}

static bool json_array_equal(struct list *a1, struct list *a2)
{
	while (!list_empty(a1) && !list_empty(a2)
			&& json_equal(list_item(a1), list_item(a2))) {
		a1 = a1->next;
		a2 = a2->next;
	}
	return list_empty(a1) && list_empty(a2);
}

static bool json_data_equal(struct json_value *v1, struct json_value *v2)
{
	switch (v1->type) {
	case JSON_OBJECT:
		return json_object_equal(v1, v2);
	case JSON_ARRAY:
		return json_array_equal(v1->array, v2->array);
	case JSON_STRING:
		return !strcmp(v1->string, v2->string);
	case JSON_INTEGER:
		return v1->integer == v2->integer;
	case JSON_FRACTION:
		return v1->fraction == v2->fraction;
	case JSON_TRUE:
	case JSON_FALSE:
	case JSON_NULL:
		return true;
	default:
		return false;
	}
}

bool json_equal(struct json_value *v1, struct json_value *v2)
{
	return v1->type == v2->type && json_data_equal(v1, v2);
}

void json_apply(struct json_value *root_object, hm_apply_func do_func, void *data)
{
	if (root_object->type != JSON_OBJECT) {
		// FIXME: this should probably get angry or something
		return;
	}
	hashmap_apply(root_object->object->values, do_func, data);
}

void json_free(struct json_value *value)
{
	switch (value->type) {
	case JSON_OBJECT:
		hashmap_free(value->object->values, false, (free_item_func) json_free);
		free(value->object);
		break;
	case JSON_ARRAY:;
		struct list *array = value->array;
		while (!list_empty(array)) {
			json_free(list_remove(array));
		}
		free(array);
		break;
	case JSON_STRING:
		break;
	default:
		break;
	}
	free(value);
}
