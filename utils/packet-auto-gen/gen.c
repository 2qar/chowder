#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "parser.h"
#include "protocol_types.h"

void put_id(const char *packet_name, int id)
{
	printf("#define PROTOCOL_ID_%s 0x%02x\n", packet_name, id);
}

void put_includes()
{
	puts(
		"#include <assert.h>\n"
		"#include <stdbool.h>\n"
		"#include <string.h>\n"
		"#include <stdint.h>\n"
		"#include <stdio.h>\n"
		"#include <stdlib.h>\n"
		"#include \"protocol_types.h\"\n"
		"#include \"conn.h\"\n"
		"#include \"packet.h\""
	);
}

static char *ftype_to_ctype(struct field *f)
{
	static char *primitive_ftype_ctypes[] = {
		[FT_BOOL] = "bool",
		[FT_BYTE] = "int8_t",
		[FT_UBYTE] = "uint8_t",
		[FT_SHORT] = "int16_t",
		[FT_USHORT] = "uint16_t",
		[FT_INT] = "int32_t",
		[FT_LONG] = "int64_t",
		[FT_FLOAT] = "float",
		[FT_DOUBLE] = "double",
		[FT_STRING] = "char *",
		[FT_CHAT] = "char *",
		[FT_IDENTIFIER] = "char *",
		[FT_VARINT] = "int32_t",
		[FT_VARLONG] = "int64_t",
		[FT_NBT] = "struct nbt *",
		[FT_POSITION] = "int64_t",
		[FT_ANGLE] = "uint8_t",
		[FT_UUID] = "uint64_t",
	};
	switch (f->type) {
		case FT_ARRAY:
		case FT_BYTE_ARRAY:
			return ftype_to_ctype(f->array.type_field);
	        case FT_BYTE_ARRAY_LEN:
			return ftype_to_ctype(f->byte_array_len_field);
		case FT_ENUM:
			if (f->enum_data.type_field->type == FT_STRING)
				return "char*";
			else
				return "enum";
		case FT_ENTITY_METADATA:
			return NULL;
		case FT_SLOT:
			if (f->is_array_type_field)
				return "struct slot";
			else
				return "struct slot *";
		default:
			assert(f->type < sizeof(primitive_ftype_ctypes) / sizeof(char *)
					&& primitive_ftype_ctypes[f->type] != NULL);
			return primitive_ftype_ctypes[f->type];
	}
}

static void put_indent(size_t indent)
{
	for (size_t i = 0; i < indent; ++i)
		putchar('\t');
}

static int put_indented(size_t indent, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	put_indent(indent);
	int n = vprintf(fmt, ap);
	va_end(ap);
	return n;
}

static void put_upper(char *s)
{
	while (*s != '\0') {
		if (isalpha(*s))
			putchar(toupper(*s));
		else
			putchar(*s);
		++s;
	}
}

static void put_enum_constant(char *packet_name, char *enum_name, char *constant)
{
	put_upper(packet_name);
	putchar('_');
	put_upper(enum_name);
	putchar('_');
	put_upper(constant);
}

static void put_enum(char *packet_name, struct field *f, size_t indent)
{
	put_indented(indent, "enum {\n");
	struct enum_constant *c = f->enum_data.constants;
	while (c != NULL) {
		put_indent(indent + 1);
		put_enum_constant(packet_name, f->name, c->name);
		if (c->has_value)
			printf(" = %d", c->value);
		printf(",\n");
		c = c->next;
	}
	put_indented(indent, "} %s;\n", f->name);
}

static void put_fields(char *packet_name, struct field *, size_t indent);

static void put_struct(char *packet_name, struct field *f, size_t indent)
{
	put_indented(indent, "struct {\n");
	put_fields(packet_name, f->struct_fields, indent + 1);
	put_indented(indent, "} %s;\n", f->name);
}

static void put_field(char *packet_name, struct field *f, size_t indent)
{
	switch (f->type) {
		case FT_BYTE_ARRAY:
			if (f->array.type_field)
				put_fields(packet_name, f->array.type_field, indent);
			else
				put_indented(indent, "uint8_t *%s;\n", f->name);
			break;
		case FT_ENUM:
			if (f->enum_data.type_field->type != FT_STRING)
				put_enum(packet_name, f, indent);
			else
				put_indented(indent, "char *%s;\n", f->name);
			break;
		case FT_STRUCT:
			put_struct(packet_name, f, indent);
			break;
		case FT_STRUCT_ARRAY:
			put_indented(indent, "struct %s_%s *%s;\n",
					packet_name, f->struct_array.struct_name, f->name);
			break;
		case FT_ARRAY:
			put_indented(indent, "%s *%s;\n",
					ftype_to_ctype(f->array.type_field), f->name);
			break;
		case FT_UNION:
			put_indented(indent, "union {\n");
			put_fields(packet_name, f->union_data.fields, indent + 1);
			put_indented(indent, "}");
			if (f->name)
				printf(" %s", f->name);
			printf(";\n");
			break;
		case FT_UUID:
			put_indented(indent, "uint64_t %s[2];\n", f->name);
			break;
		default:
			put_indented(indent, "%s %s;\n", ftype_to_ctype(f), f->name);
			break;
	}
}

static void put_fields(char *name, struct field *f, size_t indent)
{
	while (f->type) {
		if (f->type != FT_EMPTY)
			put_field(name, f, indent);
		f = f->next;
	}
}

static void generate_struct(char *name, struct field *fields)
{
	printf("struct %s {\n", name);
	put_fields(name, fields, 1);
	printf("};\n");
}

static void generate_struct_array_structs(char *name, struct field *f)
{
	while (f->type != 0) {
		switch (f->type) {
			case FT_BYTE_ARRAY:
				if (f->array.type_field)
					generate_struct_array_structs(name, f->array.type_field);
				break;
			case FT_STRUCT:
				generate_struct_array_structs(name, f->struct_fields);
				break;
			case FT_UNION:
				generate_struct_array_structs(name, f->union_data.fields);
				break;
			case FT_STRUCT_ARRAY:
				generate_struct_array_structs(name, f->struct_array.fields);
				break;
			default:
				break;
		}

		if (f->type == FT_STRUCT_ARRAY) {
			printf("struct %s_%s {\n", name, f->struct_array.struct_name);
			put_fields(name, f->struct_array.fields, 1);
			printf("};\n");
		}

		f = f->next;
	}
}

void generate_structs(char *name, struct field *fields)
{
	generate_struct_array_structs(name, fields);
	generate_struct(name, fields);
}

enum signature_type {
	READ_SIGNATURE,
	WRITE_SIGNATURE,
};

static void put_function_signature(char *packet_name, enum signature_type type)
{
	char *read_write;
	char *data_name;
	switch (type) {
		case READ_SIGNATURE:
			read_write = "read";
			data_name = "**packptr";
			break;
		case WRITE_SIGNATURE:
			read_write = "write";
			data_name = "*pack";
			break;
	}
	printf("struct protocol_err protocol_%s_%s(struct packet *p, struct %s %s)",
			read_write, packet_name, packet_name, data_name);
}

void put_function_signatures(char *packet_name)
{
	put_function_signature(packet_name, WRITE_SIGNATURE);
	printf(";\n");
	put_function_signature(packet_name, READ_SIGNATURE);
	printf(";\n");
}

static char *ftype_to_packet_type(uint32_t ft)
{
	switch (ft) {
		case FT_BOOL:
		case FT_BYTE:
		case FT_UBYTE:
			return "byte";
		case FT_SHORT:
		case FT_USHORT:
			return "short";
		case FT_INT:
			return "int";
		case FT_VARINT:
			return "varint";
		case FT_LONG:
		case FT_POSITION:
			return "long";
		case FT_FLOAT:
			return "float";
		case FT_DOUBLE:
			return "double";
		case FT_STRING:
		case FT_IDENTIFIER:
			return "string";
		case FT_UUID:
			return "bytes";
		case FT_SLOT:
			return "slot";
		case FT_NBT:
			return "nbt";
		default:
			return NULL;
	}
}

static void put_path_iter(struct field *f)
{
	if (f == NULL) {
		printf("pack->");
	} else {
		put_path_iter(f->parent);

		if (f->name != NULL) {
			printf("%s", f->name);
			if (f->type == FT_STRUCT_ARRAY)
				printf("[i_%s]", f->name);
			putchar('.');
		}
	}
}

static void put_path(struct field *f)
{
	put_path_iter(f->parent);
}

static void put_full_field_name(struct field *f)
{
	put_path(f);
	printf("%s", f->name);
	if (f->is_array_type_field)
		printf("[i_%s]", f->name);
}

#define put_input_error(indent, input_err, field) \
	put_indented(indent, "err.err_type = PROTOCOL_ERR_INPUT;\n"); \
	put_indented(indent, "err.input_err.err_type = %s;\n", #input_err); \
	put_indented(indent, "err.input_err.field_name = \"%s\";\n", field->name); \
	put_indented(indent, "return err;\n"); \

static void put_bitcount(struct field *f, size_t indent)
{
	char *len_name = f->struct_array.len_field->name;
	put_indented(indent, "size_t %s_bits = ", len_name);
	put_path(f->struct_array.len_field);
	printf("%s;\n", len_name);
	// FIXME: size_t is just the type for object sizes (arrays, etc),
	//        not actually the biggest integer type? who woulda thunk
	//        'size_t' is related to size xd
	put_indented(indent, "size_t i_%s = 0;\n", len_name);
	put_indented(indent, "while (i_%s < sizeof(size_t)*8 && %s_bits != 0) {\n", len_name, len_name);
	put_indented(indent + 1, "%s_len += %s_bits & 1;\n", f->name, len_name);
	put_indented(indent + 1, "%s_bits >>= 1;\n", len_name);
	put_indented(indent + 1, "++i_%s;\n", len_name);
	put_indented(indent, "}\n");
}

static void put_struct_array_len(struct field *f, size_t indent)
{
	put_indented(indent, "%s %s_len = ", ftype_to_ctype(f->struct_array.len_field), f->name);
	if (f->struct_array.len_is_bitcount) {
		printf("0;\n");
		put_bitcount(f, indent);
	} else {
		put_path(f);
		printf("%s_len;\n", f->name);
	}
}

static void put_array(char *packet_name, const char *packet_var, struct field *f, size_t indent,
		void (*read_write_func)(char *, const char *, struct field *, size_t))
{
	if (f->array.has_len) {
		put_indented(indent, "for (size_t i_%s = 0; i_%s < %zu; ++i_%s) {\n", f->name, f->name, f->array.array_len, f->name);
	} else {
		put_indented(indent, "for (%s i_%s = 0; i_%s < ", ftype_to_ctype(f->array.len_field), f->name, f->name);
		put_path(f->array.len_field);
		printf("%s; ++i_%s) {\n", f->array.len_field->name, f->name);
	}
	read_write_func(packet_name, packet_var, f->array.type_field, indent + 1);
	put_indented(indent, "}\n");
}

static void write_field(char *packet_name, const char *packet_var, struct field *, size_t indent);
static void write_fields(char *packet_name, const char *packet_var, struct field *, size_t indent);

static void write_byte_array(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	if (f->array.type_field) {
		put_indented(indent, "struct packet byte_array_pack;\n");
		put_indented(indent, "packet_init(&byte_array_pack);\n");
		put_indented(indent, "byte_array_pack.packet_mode = PACKET_MODE_WRITE;\n");
		write_fields(packet_name, "(&byte_array_pack)", f->array.type_field, indent);
		put_indent(indent);
		put_path(f->array.len_field);
		printf("%s = byte_array_pack.packet_len;\n", f->array.len_field->name);
		write_field(packet_name, packet_var, f->array.len_field->byte_array_len_field, indent);
		put_indented(indent, "n = packet_write_bytes(%s, byte_array_pack.packet_len, byte_array_pack.data);\n", packet_var);
		put_indented(indent, "if (n < 0)\n");
		put_indented(indent + 1, "goto err;\n");
		put_indented(indent, "free(byte_array_pack.data);\n");
	} else {
		put_indented(indent, "n = packet_write_bytes(%s, ", packet_var);
		if (f->array.has_len) {
			printf("%zu", f->array.array_len);
		} else {
			put_full_field_name(f->array.len_field);
		}
		printf(", ");
		put_full_field_name(f);
		printf(");\n");
	}
}

static void write_string_enum_check(struct field *enum_field, size_t indent)
{
	put_indented(indent, "static const char *%s_values[] = { ", enum_field->name);
	struct enum_constant *c = enum_field->enum_data.constants;
	while (c != NULL) {
		printf("\"%s\", ", c->name);
		c = c->next;
	}
	printf("NULL };\n");

	put_indented(indent, "size_t i_%s = 0;\n", enum_field->name);
	put_indented(indent, "while (%s_values[i_%s] != NULL && strcmp(%s_values[i_%s], ", enum_field->name, enum_field->name, enum_field->name, enum_field->name);
	put_path(enum_field);
	printf("%s))\n", enum_field->name);
	put_indented(indent + 1, "++i_%s;\n", enum_field->name);

	put_indented(indent, "if (%s_values[i_%s] == NULL) {\n", enum_field->name, enum_field->name);
	put_indented(indent + 1, "err.input_err.enum_constant = ");
	put_path(enum_field);
	printf("%s;\n", enum_field->name);
	put_input_error(indent + 1, PROTOCOL_INPUT_ERR_BAD_ENUM_CONSTANT, enum_field);
	put_indented(indent, "}\n");
}

static void write_struct_array(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	put_struct_array_len(f, indent);
	put_indented(indent, "for (%s i_%s = 0; i_%s < %s_len; ++i_%s) {\n", ftype_to_ctype(f->struct_array.len_field), f->name, f->name, f->name, f->name);
	write_fields(packet_name, packet_var, f->struct_array.fields, indent + 1);
	put_indent(indent);
	printf("}\n");
}

static void write_union(char *packet_name, const char *packet_var, struct field *union_field, size_t indent)
{
	struct field *enum_field = union_field->union_data.enum_field;
	put_indented(indent, "switch (");
	put_path(enum_field);
	printf("%s) {\n", enum_field->name);
	struct field *f = union_field->union_data.fields;
	struct enum_constant *c = enum_field->enum_data.constants;
	// FIXME: check during parsing or some other stage that
	//        the enum's constants_len == union_data.fields len,
	//        and that the values are 0 -> len-1
	while (f->type != 0 && c != NULL) {
		put_indented(indent + 1, "case ");
		// FIXME: re-writing the constants again with the packet name here
		//        feels wrong, the enum constants should be re-written
		//        with the packet name as a part of them during parsing
		//        or some other stage so the packet_name doesn't need to
		//        be passed around everywhere just for this function
		put_enum_constant(packet_name, enum_field->name, c->name);
		printf(":;\n");
		write_field(packet_name, packet_var, f, indent + 2);
		put_indented(indent + 2, "break;\n");
		f = f->next;
		c = c->next;
	}

	// for each enum constant,
	//   if the corresponding field isn't an empty,
	//     write it out
	put_indent(indent);
	printf("}\n");
}

static void write_string(const char *packet_var, struct field *f, size_t indent)
{
	put_indented(indent, "size_t %s_len = strlen(", f->name);
	put_path(f);
	printf("%s);\n", f->name);
	put_indented(indent, "if (%s_len > %zu) {\n", f->name, f->string_max_len);
	put_indented(indent + 1, "err.input_err.len.max = %zu;\n", f->string_max_len);
	put_indented(indent + 1, "err.input_err.len.value = %s_len;\n", f->name);
	put_input_error(indent + 1, PROTOCOL_INPUT_ERR_LEN, f);
	put_indented(indent, "}\n");

	put_indented(indent, "n = packet_write_string(%s, %s_len, ", packet_var, f->name);
	put_path(f);
	printf("%s);\n", f->name);
}

static void write_slot(const char *packet_var, struct field *f, size_t indent)
{
	put_indented(indent, "n = packet_write_slot(%s, ", packet_var);
	if (f->is_array_type_field)
		putchar('&');
	put_full_field_name(f);
	printf(");\n");
	put_indented(indent, "if (n < 0)\n");
	put_indented(indent + 1, "goto err;\n");
}

static void write_uuid(const char *packet_var, struct field *f, size_t indent)
{
	put_indented(indent, "n = packet_write_bytes(%s, 16, ", packet_var);
	put_path(f);
	printf("%s);\n", f->name);
}

static void put_string(size_t len, const char *s)
{
	for (size_t i = 0; i < len; ++i)
		putchar(s[i]);
}

static void put_operand(struct condition *condition, size_t i)
{
	if (condition->operands[i].is_field) {
		put_path(condition->operands[i].field);
		printf("%s", condition->operands[i].field->name);
	} else
		put_string(condition->operands[i].string_len, condition->operands[i].string);
}

static void put_condition(struct condition *condition)
{
	put_operand(condition, 0);
	if (condition->op) {
		printf(" %s ", condition->op);
		put_operand(condition, 1);
	}

}

static void write_field(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	if (f->condition != NULL) {
		put_indented(indent, "if (");
		put_condition(f->condition);
		printf(") {\n");
		++indent;
	}
	bool check_result = false;
	switch (f->type) {
		case FT_ARRAY:
			put_array(packet_name, packet_var, f, indent, &write_field);
			break;
		case FT_BYTE_ARRAY:
			write_byte_array(packet_name, packet_var, f, indent);
			break;
		case FT_ENUM:
			if (f->enum_data.type_field->type == FT_STRING)
				write_string_enum_check(f, indent);
			write_field(packet_name, packet_var, f->enum_data.type_field, indent);
			break;
		case FT_STRUCT:;
			write_fields(packet_name, packet_var, f->struct_fields, indent);
			break;
		case FT_STRUCT_ARRAY:
			write_struct_array(packet_name, packet_var, f, indent);
			break;
		case FT_UNION:
			write_union(packet_name, packet_var, f, indent);
			break;
		case FT_STRING:
		case FT_IDENTIFIER:
		case FT_CHAT:
			write_string(packet_var, f, indent);
			check_result = true;
			break;
		case FT_SLOT:
			write_slot(packet_var, f, indent);
			break;
		case FT_UUID:
			write_uuid(packet_var, f, indent);
			check_result = true;
			break;
		case FT_EMPTY:
			break;
		default:;
			char *packet_type = ftype_to_packet_type(f->type);
			if (packet_type != NULL) {
				put_indented(indent, "n = packet_write_%s(%s, ", packet_type, packet_var);
				put_full_field_name(f);
				printf(");\n");
				check_result = true;
			}
			break;
	}
	if (check_result) {
		put_indented(indent, "if (n < 0)\n");
		put_indented(indent + 1, "goto err;\n");
	}
	if (f->condition != NULL) {
		--indent;
		put_indented(indent, "}\n");
	}
}

static void write_fields(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	while (f->type) {
		write_field(packet_name, packet_var, f, indent);
		f = f->next;
	}
}

static void generate_write_function(int id, char *name, struct field *f)
{
	put_function_signature(name, WRITE_SIGNATURE);
	printf(" {\n");
	printf("\tmake_packet(p, 0x%x);\n", id);
	printf("\tstruct protocol_err err = {0};\n");
	printf("\tint n;\n");
	write_fields(name, "p", f, 1);
	printf("\treturn err;\n");
	printf("err:\n");
	printf("\terr.err_type = PROTOCOL_ERR_PACKET;\n");
	printf("\terr.packet_err = n;\n");
	printf("\treturn err;\n");
	printf("}\n");
}

static void read_field(char *packet_name, const char *packet_var, struct field *f, size_t indent);
static void read_fields(char *packet_name, const char *packet_var, struct field *f, size_t indent);

static void read_array(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	if (!f->array.has_len) {
		put_indented(indent, "if (");
		put_full_field_name(f->array.len_field);
		printf(" > 0)\n");
		put_indent(indent);
	}
	put_indent(indent);
	put_path(f);
	printf("%s = calloc(", f->name);
	if (f->array.has_len)
		printf("%zu", f->array.array_len);
	else {
		put_path(f->array.len_field);
		printf("%s", f->array.len_field->name);
	}
	printf(", sizeof(%s));\n", ftype_to_ctype(f));
	put_array(packet_name, packet_var, f, indent, &read_field);
}

static void read_byte_array(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	if (f->array.type_field) {
		put_indented(indent, "struct packet byte_array_pack = {0};\n");
		put_indented(indent, "byte_array_pack.packet_mode = PACKET_MODE_READ;\n");
		put_indented(indent, "byte_array_pack.data = %s->data + %s->index;\n", packet_var, packet_var);
		put_indented(indent, "byte_array_pack.packet_len = ");
		if (f->array.has_len)
			printf("%zu", f->array.array_len);
		else {
			put_path(f->array.len_field);
			printf("%s", f->array.len_field->name);
		}
		printf(";\n");
		read_fields(packet_name, "(&byte_array_pack)", f->array.type_field, indent);
		put_indented(indent, "%s->index += byte_array_pack.packet_len;\n", packet_var);
	} else {
		put_indented(indent, "const size_t %s_len = ", f->name);
		if (f->array.has_len) {
			printf("%zu", f->array.array_len);
		} else {
			put_full_field_name(f->array.len_field);
		}
		printf(";\n");
		put_indent(indent);
		put_full_field_name(f);
		printf(" = malloc(%s_len);\n", f->name);
		put_indented(indent, "if (!packet_read_bytes(%s, %s_len, ", packet_var, f->name);
		put_full_field_name(f);
		printf(")) {\n");
		put_indented(indent + 1, "err.err_type = PROTOCOL_ERR_PACKET;\n");
		put_indented(indent + 1, "err.packet_err = PACKET_TOO_BIG;\n");
		put_indented(indent + 1, "return err;\n");
		put_indented(indent, "}\n");
	}
}

static void read_varint(const char *packet_var, struct field *f, size_t indent)
{
	put_indented(indent, "n = packet_read_varint(%s, (int *) &", packet_var);
	put_full_field_name(f);
	printf(");\n");
	put_indented(indent, "if (n == PACKET_VARINT_TOO_LONG) {\n");
	put_input_error(indent + 1, PROTOCOL_INPUT_ERR_VARINT_RANGE, f);
	put_indented(indent, "} else if (n == PACKET_TOO_BIG) {\n");
	put_indented(indent + 1, "err.err_type = PROTOCOL_ERR_PACKET;\n");
	put_indented(indent + 1, "err.packet_err = n;\n");
	put_indented(indent + 1, "return err;\n");
	put_indented(indent, "}\n");
}

static void read_uuid(const char *packet_var, struct field *f, size_t indent)
{
	put_indented(indent, "if (!packet_read_bytes(%s, 16, (uint8_t *) &", packet_var);
	put_path(f);
	printf("%s)) {\n", f->name);
	put_indented(indent + 1, "err.err_type = PROTOCOL_ERR_PACKET;\n");
	put_indented(indent + 1, "err.packet_err = PACKET_TOO_BIG;\n");
	put_indented(indent + 1, "return err;\n");
	put_indented(indent, "}\n");
}

static void read_string(const char *packet_var, struct field *f, size_t indent)
{
	put_indented(indent, "size_t %s_len = %zu;\n", f->name, f->string_max_len + 1);
	put_indent(indent);
	put_path(f);
	printf("%s = calloc(%s_len, sizeof(char));\n", f->name, f->name);
	put_indented(indent, "n = packet_read_string(%s, %s_len, ", packet_var, f->name);
	put_path(f);
	printf("%s);\n", f->name);
	put_indented(indent, "if (n == PACKET_VARINT_TOO_LONG) {\n");
	put_input_error(indent + 1, PROTOCOL_INPUT_ERR_VARINT_RANGE, f);
	put_indented(indent, "} else if (n < 0) {\n");
	put_indented(indent + 1, "err.err_type = PROTOCOL_ERR_PACKET;\n");
	put_indented(indent + 1, "err.packet_err = n;\n");
	put_indented(indent + 1, "return err;\n");
	put_indented(indent, "}\n");
}

static void read_union(char *packet_name, const char *packet_var, struct field *union_field, size_t indent)
{
	put_indented(indent, "switch (");
	put_path(union_field->union_data.enum_field);
	printf("%s) {\n", union_field->union_data.enum_field->name);

	struct enum_constant *c = union_field->union_data.enum_field->enum_data.constants;
	struct field *f = union_field->union_data.fields;
	while (c != NULL && f->type) {
		put_indented(indent + 1, "case ");
		put_enum_constant(packet_name, union_field->union_data.enum_field->name, c->name);
		printf(":;\n");
		read_field(packet_name, packet_var, f, indent + 2);
		put_indented(indent + 2, "break;\n");

		c = c->next;
		f = f->next;
	}
	put_indented(indent, "}\n");
}

static void read_struct_array(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	put_struct_array_len(f, indent);
	put_indented(indent, "if (%s_len > 0)\n", f->name);
	put_indent(indent + 1);
	put_path(f);
	printf("%s = calloc(%s_len, sizeof(struct %s_%s));\n", f->name, f->name, packet_name, f->struct_array.struct_name);
	put_indented(indent, "for (%s i_%s = 0; i_%s < %s_len; ++i_%s) {\n", ftype_to_ctype(f->struct_array.len_field), f->name,  f->name, f->name, f->name);
	read_fields(packet_name, packet_var, f->struct_array.fields, indent + 1);
	put_indented(indent, "}\n");
}

static void read_slot(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	(void) packet_name;
	if (!f->is_array_type_field) {
		put_indent(indent);
		put_full_field_name(f);
		printf(" = calloc(1, sizeof(struct slot));\n");
	}
	put_indented(indent, "n = packet_read_slot(%s, ", packet_var);
	if (f->is_array_type_field) {
		putchar('&');
	}
	put_full_field_name(f);
	printf(");\n");
	put_indented(indent, "if (n < 0)\n");
	put_indented(indent + 1, "return err;\n");
}

static void read_field(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	switch (f->type) {
		case FT_ARRAY:
			read_array(packet_name, packet_var, f, indent);
			break;
		case FT_BYTE_ARRAY:
			read_byte_array(packet_name, packet_var, f, indent);
			break;
	        case FT_BYTE_ARRAY_LEN:
			read_field(packet_name, packet_var, f->byte_array_len_field, indent);
			break;
		case FT_ENUM:
			read_field(packet_name, packet_var, f->enum_data.type_field, indent);
			break;
		case FT_VARINT:
			read_varint(packet_var, f, indent);
			break;
		case FT_UUID:
			read_uuid(packet_var, f, indent);
			break;
		case FT_STRING:
			read_string(packet_var, f, indent);
			break;
		case FT_UNION:
			read_union(packet_name, packet_var, f, indent);
			break;
		case FT_STRUCT_ARRAY:
			read_struct_array(packet_name, packet_var, f, indent);
			break;
		case FT_STRUCT:
			read_fields(packet_name, packet_var, f->struct_fields, indent);
			break;
		case FT_SLOT:
			read_slot(packet_name, packet_var, f, indent);
			break;
		default:;
			char *packet_type = ftype_to_packet_type(f->type);
			if (packet_type != NULL) {
				put_indented(indent, "n = packet_read_%s(%s, ", packet_type, packet_var);
				switch (f->type) {
					case FT_BOOL:
					case FT_BYTE:
						printf("(uint8_t *) ");
						break;
					case FT_SHORT:
						printf("(uint16_t *) ");
						break;
				        case FT_LONG:
					case FT_POSITION:
						printf("(uint64_t *) ");
					        break;
					default:
						break;
				}
				putchar('&');
				put_full_field_name(f);
				printf(");\n");
				put_indented(indent, "if (n < 0)\n");
				put_indented(indent + 1, "return err;\n");
			}
			break;
	}
}

static void read_fields(char *packet_name, const char *packet_var, struct field *f, size_t indent)
{
	while (f->type) {
		read_field(packet_name, packet_var, f, indent);
		f = f->next;
	}
}

static void generate_read_function(int id, char *packet_name, struct field *root)
{
	put_function_signature(packet_name, READ_SIGNATURE);
	printf(" {\n");
	printf("\tassert(p->packet_mode == PACKET_MODE_READ);\n");
	printf("\tassert(p->packet_id == 0x%x);\n", id);
	printf("\tstruct protocol_err err = {0};\n");
	printf("\tif (*packptr == NULL)\n");
	printf("\t\t*packptr = malloc(sizeof(struct %s));\n", packet_name);
	printf("\tstruct %s *pack = *packptr;\n", packet_name);
	printf("\tint n;\n");
	read_fields(packet_name, "p", root, 1);
	printf("\treturn err;\n");
	printf("}\n");
}

void generate_source(int id, char *packet_name, struct field *head)
{
	printf("#include \"%s.h\"\n", packet_name);
	generate_write_function(id, packet_name, head);
	generate_read_function(id, packet_name, head);
}
