#ifndef PARSER_CONF_H
#define PARSER_CONF_H

#include <stdbool.h>
#include <stdint.h>
#include "parser.h"

const uint32_t valid_types[] = {
	FT_BOOL,
	FT_BYTE,
	FT_UBYTE,
	FT_SHORT,
	FT_USHORT,
	FT_INT,
	FT_LONG,
	FT_FLOAT,
	FT_DOUBLE,
	FT_STRING,
	FT_CHAT,
	FT_IDENTIFIER,
	FT_VARINT,
	FT_VARLONG,
	FT_ENTITY_METADATA,
	FT_SLOT,
	FT_NBT,
	FT_POSITION,
	FT_ANGLE,
	FT_UUID,
	FT_ARRAY,
	FT_ENUM,
	FT_BYTE_ARRAY,
	FT_UNION,
	FT_STRUCT,
	FT_EMPTY,
	0,
};

const uint32_t valid_enum_types[] = {
	FT_INT,
	FT_VARINT,
	FT_STRING,
	0,
};

const uint32_t valid_types_with_args[] = {
	FT_ARRAY,
	FT_BYTE_ARRAY,
	FT_ENUM,
	FT_STRING,
	FT_UNION,
	0,
};

const uint32_t types_with_optional_args[] = {
	FT_BYTE_ARRAY,
	0,
};

#endif // PARSER_CONF_H
