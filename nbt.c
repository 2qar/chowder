#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "nbt.h"

void nbt_write_byte(struct nbt *n, uint8_t byte) {
	/* FIXME: verify n->_index < MAX_NBT_LEN */
	n->data[n->_index++] = byte;
}

void nbt_write_short_tagless(struct nbt *n, int16_t s) {
	uint16_t ns = htons(s);
	/*
	nbt_write_byte(n, ns >> 8);
	nbt_write_byte(n, ns & 0xff);
	*/
	memcpy(n->data + n->_index, &ns, 2);
	n->_index += 2;
}

void nbt_write_cstring(struct nbt *n, const char *s) {
	size_t s_len = strlen(s);
	/* FIXME: this probably breaks if s is too long */
	nbt_write_short_tagless(n, (int16_t) s_len);
	if (s_len > 0) {
		memcpy(n->data + n->_index, s, s_len);
		n->_index += s_len;
	}
}

void nbt_write_tag(struct nbt *n, enum tag t, const char *name) {
	nbt_write_byte(n, t);
	nbt_write_cstring(n, name);
}

void nbt_init(struct nbt *n) {
	n->_index = 0;
	if (n->data == NULL)
		n->data = malloc(DEFAULT_NBT_LEN);
}

void nbt_write_init(struct nbt *n, const char *name) {
	nbt_init(n);
	nbt_write_tag(n, TAG_Compound, name);
}

void nbt_finish(struct nbt *n) {
	nbt_write_byte(n, TAG_End);
}

void nbt_write_int_tagless(struct nbt *n, int32_t i) {
	uint32_t ni = htonl(i);
	memcpy(n->data + n->_index, &ni, 4);
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

uint8_t nbt_read_byte(struct nbt *n) {
	/* TODO: bounds-checking */
	return n->data[n->_index++];
}

int16_t nbt_read_short(struct nbt *n) {
	int16_t i = nbt_read_byte(n) << 8;
	i += nbt_read_byte(n);
	return i;
}

void nbt_read_string(struct nbt *n, uint16_t len, char *s) {
	for (uint16_t i = 0; i < len; ++i)
		s[i] = nbt_read_byte(n);
	s[len] = 0;
}

int nbt_read_tag_name(struct nbt *n, uint16_t buf_len, char *s) {
	int16_t len = nbt_read_short(n);
	if (!len)
		return -1;
	uint16_t read = 0;
	for (int16_t i = 0; i < len; ++i) {
		if (i < buf_len-1) {
			s[i] = nbt_read_byte(n);
			++read;
		}
		else {
			nbt_read_byte(n);
		}
	}
	s[len] = 0;
	return read;
}

int32_t nbt_read_int(struct nbt *n) {
	int32_t v = 0;
	for (int i = 3; i >= 0; --i)
		v += nbt_read_byte(n) << (8 * i);
	return v;
}

int nbt_tag_len(struct nbt *n, enum tag t) {
	switch (t) {
		case TAG_Byte:
			return 1;
		case TAG_Short:
			return 2;
		case TAG_Int:
			return 4;
		case TAG_Long:
			return 8;
		case TAG_Float:
			return 4;
		case TAG_Double:
			return 8;
		case TAG_Byte_Array:
			return nbt_read_int(n);
		case TAG_String:
			return nbt_read_short(n);
		case TAG_Int_Array:
			return nbt_read_int(n) * 4;
		case TAG_Long_Array:
			return nbt_read_int(n) * 8;
		default:
			return 0;
	}
}

/* TODO: add helper functions for parsing compounds, seeking compound end, parsing list of compounds */

int nbt_tag_seek_iter(struct nbt *n, enum tag t, const char *name, uint8_t compound_level) {
	char buf[32] = {0};
	do {
		uint16_t tag_start = n->_index;
		enum tag current_tag = nbt_read_byte(n);
		if (current_tag != TAG_End) {
			int len = nbt_read_tag_name(n, 32, buf);
			if (t == current_tag && !strncmp(buf, name, len)) {
				n->_index = tag_start;
				return tag_start;
			}
		}

		/* no match, skip this tag */
		switch (current_tag) {
			case TAG_End:
				--compound_level;
				break;
			case TAG_List:
				;
				enum tag tag = nbt_read_byte(n);
				uint8_t list_len = nbt_read_int(n);
				if (tag == TAG_Compound) {
					for (uint8_t i = 0; i < list_len; ++i) {
						int index = nbt_tag_seek_iter(n, t, name, 1);
						if (index != -1)
							return index;
					}
				} else if (tag != TAG_End) {
					n->_index += list_len * nbt_tag_len(n, tag);
				}
				break;
			case TAG_Compound:
				++compound_level;
				break;
			default:
				n->_index += nbt_tag_len(n, current_tag);
				break;
		}
	} while (compound_level > 0);

	return -1;
}

int nbt_tag_seek(struct nbt *n, enum tag t, const char *name) {
	return nbt_tag_seek_iter(n, t, name, 0);
}
