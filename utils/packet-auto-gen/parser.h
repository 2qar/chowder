#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

// https://wiki.vg/Protocol#Data_types
// Values are (mostly) FNV-1a hashes for parsing
enum field_type {
	FT_BOOL = 0x0329,
	FT_BYTE = 0x5206,
	FT_UBYTE = 0xc4ab,
	FT_SHORT = 0x82a8,
	FT_USHORT = 0xcba0,
	FT_INT = 0xed8a,
	FT_LONG = 0xd1e0,
	FT_FLOAT = 0x2ea4,
	FT_DOUBLE = 0xc26e,
	FT_STRING = 0x2817,
	FT_CHAT = 0xfab2,
	FT_IDENTIFIER = 0xd220,
	FT_VARINT = 0x3fb6,
	FT_VARLONG = 0x5883,
	FT_ENTITY_METADATA = 0xf261,
	FT_SLOT = 0x6350,
	FT_NBT = 0xc7d5,
	FT_POSITION = 0xd655,
	FT_ANGLE = 0xaa7c,
	FT_UUID = 0xad49,
	FT_ARRAY = 0xea0e,
	FT_ENUM = 0x326d,
	FT_BYTE_ARRAY = 0x122c,

	// These types aren't part of the protocol, but I need them for generating C structs
	// and generating read/write code
	FT_UNION = 0x0d2a,
	FT_STRUCT = 0x2ce2,
	FT_STRUCT_ARRAY = 17,
	FT_EMPTY = 0xced9,
	// used internally in searching
	FT_ANY = 18,
	FT_BYTE_ARRAY_LEN = 19,
};

// only supports one binary operation
struct condition {
	struct  {
		bool is_field;
		union {
			struct field *field;
			struct {
				size_t string_len;
				char *string;
			};
		};
	} operands[2];
	char *op;
};

struct enum_constant {
	char *name;
	bool has_value;
	int value;
	struct enum_constant *next;
};

struct field {
	enum field_type type;
	char *name;
	struct condition *condition;
	bool is_array_type_field;
	union {
		size_t string_max_len;
		struct {
			struct field *type_field;
			struct enum_constant *constants;
		} enum_data;
		struct {
			struct field *enum_field;
			struct field *fields;
		} union_data;
		struct field *struct_fields;
		struct {
			struct field *type_field;
			bool has_len;
			union {
				size_t array_len;
				struct field *len_field;
			};
		} array;
		struct {
			char *struct_name;
			struct field *fields;
			struct field *len_field;
			bool len_is_bitcount;
		} struct_array;
		struct field *byte_array_len_field;
	};
	struct field *parent;
	struct field *next;
};

struct arg {
	enum {
		ARG_TYPE_NUM,
		ARG_TYPE_FIELD_TYPE,
		ARG_TYPE_FIELD_REF,
		ARG_TYPE_STRUCT,
		ARG_TYPE_BITCOUNT,
	} type;
	struct token *start_token;
	union {
		size_t num;
		struct field *field;
		struct token *struct_name;
		struct arg *bitcount_arg;
	};
	struct arg *next;
};

struct token *parse_field(struct token *, struct field *);

void create_parent_links(struct field *root);
bool resolve_field_name_refs(struct field *root);

void free_fields(struct field *);
void free_args(struct arg *);

#endif // PARSER_H
