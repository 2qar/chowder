#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "lexer.h"
#include "parser.h"
#include "parser_conf.h"

static void put_token(FILE *f, struct token *t)
{
	for (size_t i = 0; i < t->len; ++i)
		fputc(t->start[i], f);
}

static void perr(const char *s1, struct token *t, const char *s2)
{
	fprintf(stderr, "%zu:%zu | %s", t->line, t->col, s1);
	put_token(stderr, t);
	fprintf(stderr, "%s", s2);
}

static bool valid_type(uint32_t t, const uint32_t valid[])
{
	size_t i = 0;
	while (valid[i] != 0 && t != valid[i])
		++i;
	return valid[i] != 0;
}

static bool read_enum_args(struct arg *args, struct field *field)
{
	assert(args != NULL);
	if (args->type != ARG_TYPE_FIELD_TYPE) {
		perr("expected an encoding type, got '", args->start_token, "'\n");
		return true;
	} else if (args->next != NULL) {
		perr("unexpected extra argument: '", args->next->start_token, "'\n");
		return true;
	}

	field->enum_data.type_field = args->field;
	return false;
}

static bool read_array_args(struct arg *args, struct field *field)
{
	assert(args != NULL);
	if (args->type != ARG_TYPE_FIELD_TYPE) {
		perr("expected a type, got '", args->start_token, "'\n");
		return true;
	}

	args->field->is_array_type_field = true;
	if (args->field->type == FT_STRUCT) {
		field->type = FT_STRUCT_ARRAY;
		struct token *struct_name_tok = args->start_token->next;
		size_t name_len = struct_name_tok->len + 1;
		field->struct_array.struct_name = calloc(name_len, sizeof(char));
		snprintf(field->struct_array.struct_name, name_len, "%s", struct_name_tok->start);
		free(args->field->next);
		free(args->field);
	} else {
		field->array.type_field = args->field;
	}

	if (args->next != NULL && args->next->type == ARG_TYPE_NUM) {
		field->array.has_len = true;
		field->array.array_len = args->next->num;
	} else if (field->type == FT_STRUCT_ARRAY && args->next != NULL &&
			args->next->type == ARG_TYPE_BITCOUNT) {
		struct arg *bitcount_arg = args->next->bitcount_arg;
		if (bitcount_arg->type != ARG_TYPE_FIELD_REF) {
			perr("can't take the bitcount of '", bitcount_arg->start_token, "'\n");
			return true;
		} else {
			field->struct_array.len_field = bitcount_arg->field;
			bitcount_arg->field = NULL;
			field->struct_array.len_is_bitcount = true;
		}
	}
	return false;
}

static bool read_byte_array_args(struct arg *args, struct field *field)
{
	assert(args != NULL);
	if (args->type == ARG_TYPE_NUM) {
		field->array.has_len = true;
		field->array.array_len = args->num;
	} else if (args->type == ARG_TYPE_FIELD_TYPE) {
		field->array.type_field = args->field;
	} else {
		perr("expected a number or field type, got '", args->start_token, "'\n");
		return true;
	}
	return false;
}

static bool read_union_args(struct arg *args, struct field *field)
{
	assert(args != NULL);
	if (args->type != ARG_TYPE_FIELD_REF) {
		perr("expected a field name, got '", args->start_token, "'\n");
		return true;
	}
	field->union_data.enum_field = args->field;
	return false;
}

static bool read_string_args(struct arg *args, struct field *field)
{
	assert(args != NULL);
	if (args->type != ARG_TYPE_NUM) {
		perr("expected a length, got '", args->start_token, "'\n");
		return true;
	}
	field->string_max_len = args->num;
	return false;
}

static struct token *seek_next_arg_sep(struct token *arg_start)
{
	struct token *t = arg_start;
	int paren_level = 1;
	while (t != NULL && paren_level > 0 && !(token_equals(t, ",") && paren_level == 1)) {
		if (token_equals(t, "("))
			++paren_level;
		else if (token_equals(t, ")"))
			--paren_level;
		if (paren_level > 0)
			t = t->next;
	}
	return t;
}

static struct token *read_type_args(struct token *, struct field *);

static void parse_arg(struct token *arg_start, struct arg *arg)
{
	uint32_t type;
	arg->start_token = arg_start;
	if (sscanf(arg_start->start, "%zu", &arg->num) == 1) {
		arg->type = ARG_TYPE_NUM;
	} else if (valid_type((type = str_fnv1a(arg_start->start, arg_start->len)), valid_types)) {
		arg->type = ARG_TYPE_FIELD_TYPE;
		arg->field = calloc(1, sizeof(struct field));
		arg->field->next = calloc(1, sizeof(struct field));
		arg->field->type = type;
		if (valid_type(type, valid_types_with_args))
			read_type_args(arg_start, arg->field);
	} else if (token_equals(arg_start, "struct")) {
		arg->type = ARG_TYPE_STRUCT;
		arg->struct_name = arg_start->next;
	} else if (!strncmp(arg_start->start, "bitcount(", strlen("bitcount("))) {
		arg->type = ARG_TYPE_BITCOUNT;
		arg->bitcount_arg = calloc(1, sizeof(struct arg));
		parse_arg(arg_start->next->next, arg->bitcount_arg);
	} else {
		arg->type = ARG_TYPE_FIELD_REF;
		arg->field = calloc(1, sizeof(struct field));
		size_t name_len = arg_start->len + 1;
		arg->field->name = calloc(name_len, sizeof(char));
		snprintf(arg->field->name, name_len, "%s", arg_start->start);
	}
}

static struct arg *parse_args(struct token *open_paren)
{
	assert(token_equals(open_paren, "("));
	if (token_equals(open_paren->next, ")"))
		return NULL;

	struct arg *args = calloc(1, sizeof(struct arg));
	struct arg *arg = args;
	struct token *t = open_paren->next;
	struct token *next_sep = seek_next_arg_sep(t);
	while (!token_equals(next_sep, ")")) {
		parse_arg(t, arg);
		t = next_sep->next;
		next_sep = seek_next_arg_sep(t);

		arg->next = calloc(1, sizeof(struct arg));
		arg = arg->next;
	}
	parse_arg(t, arg);
	return args;
}

static struct token *read_type_args(struct token *type, struct field *field)
{
	struct token *open_paren = type->next;
	if (!token_equals(open_paren, "(")) {
		fprintf(stderr, "unexpected token '%c' when parsing '", open_paren->start[0]);
		put_token(stderr, type);
		fprintf(stderr, "' args\n");
		return NULL;
	}
	struct arg *args = parse_args(open_paren);
	if (args == NULL) {
		perr("expected args for '", type, "'\n");
		free_args(args);
		return NULL;
	}
	bool err = false;
	switch (field->type) {
		case FT_ENUM:
			err = read_enum_args(args, field);
			break;
		case FT_ARRAY:
			err = read_array_args(args, field);
			break;
		case FT_BYTE_ARRAY:
			err = read_byte_array_args(args, field);
			break;
		case FT_UNION:
			err = read_union_args(args, field);
			break;
		case FT_STRING:
			err = read_string_args(args, field);
			break;
		default:
			perr("'", type, "' is configured to have args, but they're not handled. FIXME\n");
			break;
	}

	if (err) {
		free_args(args);
		return NULL;
	} else if (args == NULL) {
		return open_paren->next->next;
	} else {
		struct arg *last_arg = args;
		while (last_arg->next != NULL)
			last_arg = last_arg->next;
		struct token *closing_paren = seek_next_arg_sep(last_arg->start_token);
		free_args(args);
		return closing_paren->next;
	}
}

static bool set_default_type_args(struct token *type, struct field *field)
{
	switch (field->type) {
		case FT_BYTE_ARRAY:
			field->array.has_len = false;
			break;
		default:
			perr("'", type, "' is configured to have optional args, but it has no default values. FIXME\n");
			return false;
	}
	return true;
}

static struct token *read_field_type(struct token *type, struct field *field)
{
	uint32_t field_hash = str_fnv1a(type->start, type->len);
	if (!valid_type(field_hash, valid_types)) {
		perr("unknown field type '", type, "'\n");
		return NULL;
	}

	bool has_args = valid_type(field_hash, valid_types_with_args);
	bool args_optional = valid_type(field_hash, types_with_optional_args);
	bool has_paren = token_equals(type->next, "(");
	if (has_args && !has_paren && !args_optional) {
		perr("expected args for type '", type, "'\n");
		return NULL;
	} else if (!has_args && has_paren) {
		perr("unexpected args for type '", type, "'\n");
		return NULL;
	}

	field->type = field_hash;
	struct token *name_tok = NULL;
	if (has_args && has_paren) {
		name_tok = read_type_args(type, field);
	} else if (has_args && !has_paren) {
		if (set_default_type_args(type, field)) {
			name_tok = type->next;
		}
	} else {
		switch (field->type) {
			case FT_CHAT:
				field->string_max_len = 262144;
				break;
			case FT_IDENTIFIER:
				field->string_max_len = 32767;
				break;
			default:
				break;
		}
		name_tok = type->next;
	}
	return name_tok;
}

static struct token *read_field_name(struct token *name_tok, struct field *field)
{
	if (name_tok->is_sep && field->type == FT_UNION)
		return name_tok;
	else if (name_tok->is_sep) {
		perr("expected a name, got '", name_tok, "'\n");
		return NULL;
	}


	size_t name_len = name_tok->len + 1;
	char *name = calloc(name_len, sizeof(char));
	snprintf(name, name_len, "%s", name_tok->start);
	field->name = name;
	// FIXME: it would probably be less confusing if the type fields were
	//        nameless, and if the gen code printed the parent's name for
	//        these type fields
	switch (field->type) {
		case FT_ENUM:
			field->enum_data.type_field->name = field->name;
			break;
		case FT_ARRAY:
		case FT_BYTE_ARRAY:
			if (field->array.type_field)
				field->array.type_field->name = field->name;
			break;
		default:
			break;
	}
	return name_tok->next;
}

static bool valid_conditional_seperator(char c)
{
	return c == '<' ||
		c == '>' ||
		c == '!' ||
		c == '=' ||
		c == '&' ||
		c == '|';
}

static bool is_num_literal(size_t len, const char *s)
{
	size_t i = 0;
	if (!strncmp(s, "0x", 2))
		i += 2;

	while (i < len && isdigit(s[i]))
		++i;

	return i == len;
}

static bool is_valid_operator(char c1, char c2)
{
	if (c1 == '<' || c1 == '>')
		return c2 == 0 || c2 == '=';
	else if (c1 == '&')
		return c2 == 0 || c2 == '&';
	else if (c1 == '|')
		return c2 == 0 || c2 == '|';
	else if (c1 == '=' || c1 == '!')
		return c2 == '=';
	else
		return false;
}

static struct token *read_operator(struct token *start, char **operator)
{
	struct token *end = start->next;
	char other_sep = 0;
	if (end->is_sep) {
		other_sep = *end->start;
		end = end->next;
	}

	if (!is_valid_operator(*start->start, other_sep)) {
		fprintf(stderr, "%zu:%zu | invalid operator '%c", start->line, start->col, *start->start);
		if (other_sep != 0)
			fputc(other_sep, stderr);
		fprintf(stderr, "'\n");
		return NULL;
	}

	char *op = calloc(3, sizeof(char));
	snprintf(op, 3, "%c%c", *start->start, other_sep);
	*operator = op;
	return end;
}

static void read_operand(struct token *op_start, struct condition *condition, size_t i)
{
	if (is_num_literal(op_start->len, op_start->start))
		condition->operands[i].is_field = false;
	else
		condition->operands[i].is_field = true;
	condition->operands[i].string_len = op_start->len;
	condition->operands[i].string = op_start->start;
}

static struct token *read_conditional(struct token *paren, struct field *field)
{
	struct token *cond_start = paren->next;
	if (!token_equals(cond_start, "if")) {
		perr("expected 'if' at start of conditional, got '", cond_start, "'\n");
		return NULL;
	}
	cond_start = cond_start->next;

	struct token *cond_end = cond_start;
	while (cond_end && (!cond_end->is_sep || valid_conditional_seperator(cond_end->start[0]))) {
		cond_end = cond_end->next;
	}
	if (!token_equals(cond_end, ")")) {
		perr("unexpected '", cond_end, "' in conditional\n");
		return NULL;
	} else if (cond_start->is_sep) {
		perr("expected first operand, got '", cond_start, "'\n");
		return NULL;
	}

	// FIXME: only one or two operands, no complicated logic.
	//        maybe that's for the best?
	struct condition *condition = calloc(1, sizeof(struct condition));
	read_operand(cond_start, condition, 0);

	cond_start = cond_start->next;
	if (cond_start != cond_end) {
		struct token *next_operand = read_operator(cond_start, &condition->op);
		if (next_operand == NULL) {
			return NULL;
		} else if (next_operand == cond_end) {
			perr("expected operand, got '", next_operand, "'\n");
			return NULL;
		}

		read_operand(cond_start, condition, 1);
	}

	field->condition = condition;

	return cond_end->next;
}

static struct token *parse_int_constant(struct token *constant_start, struct enum_constant *enum_constant)
{
	struct token *constant_end = NULL;
	if (token_equals(constant_start->next, "=")) {
		struct token *value_tok = constant_start->next->next;
		if (value_tok == NULL ||
				sscanf(value_tok->start, "%d\n", &enum_constant->value) != 1)
			return NULL;
		enum_constant->has_value = true;
		constant_end = value_tok;
	} else if (token_equals(constant_start->next, "\n")) {
		constant_end = constant_start;
	}

	if (constant_end != NULL) {
		size_t constant_len = constant_start->len + 1;
		enum_constant->name = calloc(constant_len, sizeof(char));
		snprintf(enum_constant->name, constant_len, "%s", constant_start->start);
		return constant_end->next->next;
	} else {
		return NULL;
	}
}

static struct token *parse_string_constant(struct token *constant_start, struct enum_constant *enum_constant)
{
	if (constant_start->start[0] != '"' ||
			constant_start->start[constant_start->len - 1] != '"' ||
			!token_equals(constant_start->next, "\n"))
		return NULL;

	size_t constant_len = constant_start->len - 1;
	enum_constant->name = calloc(constant_len, sizeof(char));
	snprintf(enum_constant->name, constant_len, "%s", constant_start->start + 1);
	return constant_start->next->next;
}

static struct token *parse_enum(struct token *first_constant, struct field *field)
{
	struct token *(*parse_constant)(struct token *, struct enum_constant *);
	if (first_constant->start[0] == '"')
		parse_constant = parse_string_constant;
	else
		parse_constant = parse_int_constant;

	struct enum_constant *constants = calloc(1, sizeof(struct enum_constant));
	struct enum_constant *constant = constants;
	struct token *c = parse_constant(first_constant, constants);
	struct token *c_prev = c;
	while (c && c->len != 0 && !token_equals(c, "}")) {
		constant->next = calloc(1, sizeof(struct enum_constant));
		constant = constant->next;
		c_prev = c;
		c = parse_constant(c, constant);
	}

	if (!c) {
		perr("invalid constant '", c_prev, "'\n");
		return NULL;
	} else if (c->len == 0) {
		fprintf(stderr, "hit EOF looking for the end of enum %s\n", field->name);
		return NULL;
	}

	field->enum_data.constants = constants;
	return c->next;
}

/* used for structs, struct arrays, unions */
static struct token *parse_fields(struct token *first_field, struct field *fields)
{
	struct token *t = first_field;
	struct field *f = fields;
	while (t && !token_equals(t, "}")) {
		t = parse_field(t, f);
		if (t)
			f = f->next;
	}
	if (t)
		return t->next;
	else
		return NULL;
}

static bool type_has_body(uint32_t ft)
{
	return ft == FT_ENUM ||
		ft == FT_STRUCT ||
		ft == FT_UNION ||
		ft == FT_STRUCT_ARRAY;
}

static struct token *read_field_body(struct token *open_brace, struct field *f)
{
	struct token *t;
	switch (f->type) {
		case FT_BYTE_ARRAY:
			t = read_field_body(open_brace, f->array.type_field);
			break;
		case FT_ENUM:
			t = parse_enum(open_brace->next->next, f);
			break;
		case FT_UNION:
		case FT_STRUCT:
		case FT_STRUCT_ARRAY:;
			struct field *fields = calloc(1, sizeof(struct field));
			t = parse_fields(open_brace->next->next, fields);
			if (f->type == FT_UNION)
				f->union_data.fields = fields;
			else if (f->type == FT_STRUCT)
				f->struct_fields = fields;
			else
				f->struct_array.fields = fields;
			break;
		default:
			break;
	}
	return t;
}

struct token *parse_field(struct token *t, struct field *f)
{
	t = read_field_type(t, f);
	if (!t)
		return NULL;
	t = read_field_name(t, f);
	if (!t)
		return NULL;
	if (token_equals(t, "("))
		t = read_conditional(t, f);
	if (!t)
		return NULL;

	bool has_body = token_equals(t, "{");
	bool should_have_body = false;
	if (f->type == FT_BYTE_ARRAY && f->array.type_field)
		should_have_body = type_has_body(f->array.type_field->type);
	else
		should_have_body = type_has_body(f->type);

	if (has_body && should_have_body) {
		t = read_field_body(t, f);
	} else if (!has_body && should_have_body) {
		fprintf(stderr, "%zu:%zu | type should have a body, but it doesn't\n",
				t->line, t->col);
		return NULL;
	} else if (has_body && !should_have_body) {
		fprintf(stderr, "%zu:%zu | type 0x%x shouldn't have a body, but it seems to have one\n",
				t->line, t->col, f->type);
		return NULL;
	}

	if (!t)
		return NULL;

	if (token_equals(t, "\n")) {
		t = t->next;
		struct field *next_field = calloc(1, sizeof(struct field));
		f->next = next_field;
	}
	return t;
}

static void create_parent_links_iter(struct field *parent, struct field *f)
{
	while (f->type != 0) {
		f->parent = parent;
		switch (f->type) {
		        case FT_ARRAY:
			case FT_BYTE_ARRAY:
				if (f->array.type_field) {
					f->array.type_field->parent = f->parent;
					create_parent_links_iter(parent, f->array.type_field);
				}
				break;
		        case FT_BYTE_ARRAY_LEN:
				create_parent_links_iter(parent, f->byte_array_len_field);
			        break;
			case FT_ENUM:
				f->enum_data.type_field->parent = f->parent;
				break;
			case FT_STRUCT:
				create_parent_links_iter(f, f->struct_fields);
				break;
			case FT_STRUCT_ARRAY:
				create_parent_links_iter(f, f->struct_array.fields);
				break;
			case FT_UNION:
				create_parent_links_iter(f, f->union_data.fields);
				break;
			default:
				break;
		}
		f = f->next;
	}
}

void create_parent_links(struct field *root)
{
	create_parent_links_iter(NULL, root);
}

// FIXME: all the stuff below seems like it belongs in a seperate file

static struct field *find_field_with_len(struct field *f, uint32_t f_type, size_t f_name_len, char *f_name)
{
	struct field *res = NULL;
	while (!res && f->type != 0) {
		if (!f->name && f->type == f_type)
			res = f;
		if (f->name && (f->type == f_type || f_type == FT_ANY) && !strncmp(f->name, f_name, f_name_len))
			res = f;
		else if (f->type == FT_STRUCT)
			res = find_field_with_len(f->struct_fields, f_type, f_name_len, f_name);
		else if (f->type == FT_STRUCT_ARRAY)
			res = find_field_with_len(f->struct_array.fields, f_type, f_name_len, f_name);
		else if (f->type == FT_UNION)
			res = find_field_with_len(f->union_data.fields, f_type, f_name_len, f_name);
		else if (f->type == FT_BYTE_ARRAY && f->array.type_field)
			res = find_field_with_len(f->array.type_field, f_type, f_name_len, f_name);

		f = f->next;
	}
	return res;
}

static struct field *find_field(struct field *f, uint32_t f_type, char *f_name)
{
	return find_field_with_len(f, f_type, strlen(f_name), f_name);
}

static bool resolve_condition_name_refs(struct field *root, struct condition *condition)
{
	size_t i = 0;
	bool err = false;
	while (!err && i < 2) {
		if (condition->operands[i].is_field) {
			struct field *f = find_field_with_len(root, FT_ANY,
					condition->operands[i].string_len,
					condition->operands[i].string);
			if (f == NULL) {
				fprintf(stderr, "couldn't put up with this shit anymore\n");
				return true;
			} else {
				condition->operands[i].field = f;
			}
		}
		++i;
	}
	return err;
}

static struct field *find_len_field(struct field *root, struct field *array_field)
{
	size_t name_len = strlen(array_field->name) + 5;
	char *name = calloc(name_len, sizeof(char));
	snprintf(name, name_len, "%s_len", array_field->name);
	struct field *len_field = find_field(root, FT_ANY, name);
	free(name);
	if (len_field == NULL)
		fprintf(stderr, "array '%s' has no given length and '%s_len' doesn't exist, gimme one\n",
				array_field->name, array_field->name);
	return len_field;
}

static bool resolve_field_name_refs_iter(struct field *root, struct field *f)
{
	bool err = false;
	while (!err && f->type != 0) {
		if (f->condition != NULL) {
			err = resolve_condition_name_refs(root, f->condition);
		}

		switch (f->type) {
			case FT_BYTE_ARRAY:
				if (f->array.type_field)
					err = resolve_field_name_refs_iter(root, f->array.type_field);
				/* fallthrough */
			case FT_ARRAY:
				if (!f->array.has_len) {
					f->array.len_field = find_len_field(root, f);
					if (f->array.len_field == NULL)
						err = true;
					else if (f->type == FT_BYTE_ARRAY && f->array.type_field) {
						struct field *len_field = malloc(sizeof(struct field));
						memcpy(len_field, f->array.len_field, sizeof(struct field));
						len_field->next = calloc(1, sizeof(struct field));
						f->array.len_field->type = FT_BYTE_ARRAY_LEN;
						f->array.len_field->byte_array_len_field = len_field;
					}
				}
				break;
			case FT_STRUCT:
				err = resolve_field_name_refs_iter(root, f->struct_fields);
				break;
			case FT_STRUCT_ARRAY:
				if (f->struct_array.len_field == NULL) {
					f->struct_array.len_field = find_len_field(root, f);
					if (f->struct_array.len_field == NULL)
						err = true;
				} else {
					char *name = f->struct_array.len_field->name;
					free(f->struct_array.len_field);
					f->struct_array.len_field = find_field(root, FT_ANY, name);
					if (f->struct_array.len_field == NULL) {
						fprintf(stderr, "the field given to bitcount() doesn't exist\n");
						err = true;
					}
					free(name);
				}
				if (!err)
					err = resolve_field_name_refs_iter(root, f->struct_array.fields);
				break;
			case FT_UNION:;
				struct field *partial = f->union_data.enum_field;
				char *name = partial->name;
				free(partial);
				struct field *enum_field = find_field(root, FT_ENUM, name);
				if (enum_field == NULL) {
					fprintf(stderr, "union '%s' depends on non-existent enum '%s'\n", f->name, name);
					err = true;
				}
				free(name);
				f->union_data.enum_field = enum_field;

				if (!err) {
					err = resolve_field_name_refs_iter(root, f->union_data.fields);
				}
				break;
			default:
				break;
		}
		f = f->next;
	}
	return err;
}

bool resolve_field_name_refs(struct field *root)
{
	return resolve_field_name_refs_iter(root, root);
}

void free_fields(struct field *f)
{
	while (f != NULL) {
		if (f->condition)
			free(f->condition->op);

		switch (f->type) {
			case FT_ARRAY:
			case FT_BYTE_ARRAY:
				if (f->array.type_field) {
					f->array.type_field->name = NULL;
					free_fields(f->array.type_field);
				}
				break;
                    	case FT_BYTE_ARRAY_LEN:
				f->name = NULL;
				free_fields(f->byte_array_len_field);
			        break;
			case FT_ENUM:
				free(f->enum_data.type_field);
				struct enum_constant *c = f->enum_data.constants;
				struct enum_constant *next;
				while (c != NULL) {
					next = c->next;
					free(c->name);
					free(c);
					c = next;
				}
				break;
			case FT_UNION:;
				free_fields(f->union_data.fields);
				break;
			case FT_STRUCT_ARRAY:
				free(f->struct_array.struct_name);
				free_fields(f->struct_array.fields);
				break;
			case FT_STRUCT:
				free_fields(f->struct_fields);
				break;
			default:
				break;
		}
		free(f->name);
		free(f->condition);
		struct field *next = f->next;
		free(f);
		f = next;
	}
}

void free_args(struct arg *args)
{
	struct arg *next_arg;
	while (args != NULL) {
		switch (args->type) {
			case ARG_TYPE_BITCOUNT:
				free_args(args->bitcount_arg);
				break;
			default:
				break;
		}
		next_arg = args->next;
		free(args);
		args = next_arg;
	}
}
