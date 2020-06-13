#include <string.h>
#include <arpa/inet.h>

#include "nbt.h"

void nbt_write_byte(struct nbt *n, uint8_t byte) {
	/* FIXME: verify n->_index < MAX_NBT_LEN */
	n->_data[n->_index++] = byte;
}

void nbt_write_short_tagless(struct nbt *n, int16_t s) {
	uint16_t ns = htons(s);
	/*
	nbt_write_byte(n, ns >> 8);
	nbt_write_byte(n, ns & 0xff);
	*/
	memcpy(n->_data + n->_index, &ns, 2);
	n->_index += 2;
}

void nbt_write_cstring(struct nbt *n, const char *s) {
	size_t s_len = strlen(s);
	/* FIXME: this probably breaks if s is too long */
	nbt_write_short_tagless(n, (int16_t) s_len);
	if (s_len > 0) {
		memcpy(n->_data + n->_index, s, s_len);
		n->_index += s_len;
	}
}

void nbt_write_tag(struct nbt *n, enum tag t, const char *name) {
	nbt_write_byte(n, t);
	nbt_write_cstring(n, name);
}

void nbt_init(struct nbt *n, const char *name) {
	n->_index = 0;
	nbt_write_tag(n, TAG_Compound, name);
}

void nbt_finish(struct nbt *n) {
	nbt_write_byte(n, TAG_End);
}

void nbt_write_int_tagless(struct nbt *n, int32_t i) {
	uint32_t ni = htonl(i);
	memcpy(n->_data + n->_index, &ni, 4);
	n->_index += 4;
}

void nbt_write_long(struct nbt *n, int64_t l) {
	/* FIXME: hacky, only works on little-endian */
	uint8_t *nl = (uint8_t *) &l;
	for (int i = sizeof(long)-1; i >= 0; --i)
		nbt_write_byte(n, nl[i]);
}

void nbt_write_long_array(struct nbt *n, const char *name, int32_t len, int64_t l[]) {
	nbt_write_tag(n, TAG_Long_Array, name);
	nbt_write_int_tagless(n, len);
	for (int i = 0; i < len; ++i)
		nbt_write_long(n, l[i]);
}
