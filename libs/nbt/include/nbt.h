/* Defines a struct for working with the NBT format.
 * Specification from https://wiki.vg/NBT#Specification.
 */
#ifndef CHOWDER_NBT_H
#define CHOWDER_NBT_H

#include <stdint.h>
#include "list.h"

enum tag {
	TAG_End,
	TAG_Byte,
	TAG_Short,
	TAG_Int,
	TAG_Long,
	TAG_Float,
	TAG_Double,
	TAG_Byte_Array,
	TAG_String,
	TAG_List,
	TAG_Compound,
	TAG_Int_Array,
	TAG_Long_Array
};

union nbt_data {
	int8_t t_byte;
	int16_t t_short;
	int32_t t_int;
	int64_t t_long;
	float t_float;
	double t_double;
	char *string;
	struct nbt_array *array;
	struct nbt_list *list;
	struct list *children;
};

struct nbt {
	enum tag tag;
	char *name;
	union nbt_data data;
};

struct nbt_array {
	enum tag type;
	int32_t len;
	union {
		int8_t *bytes;
		int32_t *ints;
		int64_t *longs;
	} data;
};

struct nbt_list {
	enum tag type;
	struct list *head;
};

struct nbt *nbt_new(enum tag, char *name);
void nbt_free(struct nbt *);

size_t nbt_unpack(size_t len, const uint8_t *b, struct nbt **out);
size_t nbt_pack(struct nbt *, uint8_t **b);

/* returns direct children only */
struct nbt *nbt_get(struct nbt *, enum tag, char *name);
/* searches the whole tree */
struct nbt *nbt_find(struct nbt *, enum tag, char *name);

#endif
