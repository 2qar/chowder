/* Defines a struct for working with the NBT format.
 * Specification from https://wiki.vg/NBT#Specification.
 */
#include <stdint.h>

#ifndef DEFAULT_NBT_LEN
#define DEFAULT_NBT_LEN 1024

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

struct nbt {
	uint16_t data_len;
	uint8_t *data;
	uint16_t _index;
};

void nbt_init(struct nbt *);
void nbt_write_init(struct nbt *, const char *name);
void nbt_finish(struct nbt *);
void nbt_write_long_array(struct nbt *, const char *name, int32_t len, int64_t[]);
int nbt_tag_seek(struct nbt *, enum tag, const char *name);

#endif
