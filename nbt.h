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
uint8_t nbt_read_byte(struct nbt *n);
void nbt_skip_tag_name(struct nbt *n);
int32_t nbt_read_int(struct nbt *n);
uint16_t nbt_read_string(struct nbt *n, uint16_t buf_len, char *buf);
int nbt_tag_seek(struct nbt *, enum tag, const char *name);
int nbt_compound_seek_tag(struct nbt *, enum tag, const char *name);
int nbt_compound_seek_end(struct nbt *);
int nbt_list_len(struct nbt *);

#endif
