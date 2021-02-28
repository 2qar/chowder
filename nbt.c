#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <endian.h>
#include "include/linked_list.h"

#include "nbt.h"

struct nbt *nbt_new(char *root_name) {
	struct nbt *root = malloc(sizeof(struct nbt));
	root->tag = TAG_Compound;
	root->name = root_name;
	return root;
}

int nbt_read_bytes(size_t n, void *dest, size_t i, size_t len, const uint8_t *data) {
	if (i < len - n) {
		memcpy(dest, data + i, n);
		return n;
	}
	return -1;
}

int nbt_read_short(int16_t *v, size_t i, size_t len, const uint8_t *data) {
	int n = nbt_read_bytes(2, v, i, len, data);
	if (n > 0) {
		*v = ntohs(*v);
	}
	return n;
}

int nbt_read_int(int32_t *v, size_t i, size_t len, const uint8_t *data) {
	int n = nbt_read_bytes(4, v, i, len, data);
	if (n > 0) {
		*v = ntohl(*v);
	}
	return n;
}

int nbt_read_long(int64_t *v, size_t i, size_t len, const uint8_t *data) {
	int n = nbt_read_bytes(8, v, i, len, data);
	if (n > 0) {
		*v = be64toh(*v);
	}
	return n;
}

size_t nbt_read_string(char **name, size_t len, const uint8_t *data) {
	size_t i = 0;

	uint16_t name_len;
	int n = nbt_read_short((int16_t *) &name_len, i, len, data);
	if (n > 0) {
		i += n;
		*name = malloc(sizeof(char) * (name_len + 1));
		n = nbt_read_bytes(name_len, *name, i, len, data);
		if (n > 0) {
			i += n;
			(*name)[n] = '\0';
		}
	}
	return i;
}

int nbt_unpack_node_data(struct nbt *, size_t, size_t, const uint8_t *);

int nbt_read_list(struct nbt_list *l, size_t len, const uint8_t *data) {
	l->type = data[0];
	int32_t list_len;
	size_t i = 1;
	int n = nbt_read_int(&list_len, i, len, data);
	if (n > 0) {
		i += n;
	}

	l->head = list_new();
	int16_t elem = 0;
	while (n > 0 && elem < list_len) {
		struct nbt *nbt = malloc(sizeof(struct nbt));
		nbt->tag = l->type;
		nbt->name = NULL;

		int n = nbt_unpack_node_data(nbt, i, len, data);
		if (n > 0) {
			i += n;
		}

		++elem;
		list_append(l->head, sizeof(struct nbt *), &nbt);
	}

	if (n <= 0) {
		return -1;
	}
	return i;
}

int nbt_read_array(struct nbt_array *a, size_t elem_bytes, size_t len, const uint8_t *data) {
	size_t i = 0;
	int n = nbt_read_int(&(a->len), i, len, data);
	if (n > 0) {
		i += n;
		if (i < len - a->len * elem_bytes) {
			a->data.bytes = malloc(a->len * elem_bytes);
			memcpy(a->data.bytes, data + i, a->len * elem_bytes);
			i += a->len * elem_bytes;
		} else {
			return -1;
		}
	} else {
		return -1;
	}
	return i;
}

ssize_t nbt_unpack_node(struct nbt *, size_t, size_t, const uint8_t *);

int nbt_unpack_node_data(struct nbt *nbt, size_t i, size_t len, const uint8_t *data) {
	int n = 0;
	switch (nbt->tag) {
		case TAG_Byte:
			if (i < len) {
				nbt->data.t_byte = data[i];
				n = 1;
			} else {
				n = -1;
			}
			break;
		case TAG_Short:
			n = nbt_read_short(&(nbt->data.t_short), i, len, data);
			break;
		case TAG_Int:
			n = nbt_read_int(&(nbt->data.t_int), i, len, data);
			break;
		case TAG_Long:
			n = nbt_read_long(&(nbt->data.t_long), i, len, data);
			break;
		case TAG_Float:
			n = nbt_read_int((int32_t *) &(nbt->data.t_float), i, len, data);
			break;
		case TAG_Double:
			n = nbt_read_long((int64_t *) &(nbt->data.t_double), i, len, data);
			break;
		case TAG_Byte_Array:
			nbt->data.array = malloc(sizeof(struct nbt_array));
			n = nbt_read_array(nbt->data.array, 1, len - i, data + i);
			break;
		case TAG_String:
			n = nbt_read_string(&(nbt->data.string), len - i, data + i);
			break;
		case TAG_List:
			nbt->data.list = malloc(sizeof(struct nbt_list));
			n = nbt_read_list(nbt->data.list, len - i, data + i);
			break;
		/* FIXME: some nodes are getting skipped, seems to happen when
		 *        nested TAG_Compounds are being read
		 */
		case TAG_Compound:
			if (i < len) {
				/* FIXME: this isn't very clear, and also
				 *        might be part of the reason why parsing
				 *        kinda breaks on compound nodes right now
				 *        but idk
				 */
				n = nbt_unpack_node(nbt, i, len, data) - i;
			} else {
				n = -1;
			}
			break;
		case TAG_Int_Array:
			nbt->data.array = malloc(sizeof(struct nbt_array));
			n = nbt_read_array(nbt->data.array, 4, len - i, data + i);
			break;
		case TAG_Long_Array:
			nbt->data.array = malloc(sizeof(struct nbt_array));
			n = nbt_read_array(nbt->data.array, 8, len - i, data + i);
			break;
		default:
			n = -2;
			break;
	}
	return n;
}

ssize_t nbt_unpack_node(struct nbt *root, size_t i, size_t len, const uint8_t *data) {
	root->data.children = list_new();
	bool valid_nbt = true;
	while (valid_nbt && i < len && data[i] != TAG_End) {
		struct nbt *child = malloc(sizeof(struct nbt));
		child->tag = data[i];
		++i;
		i += nbt_read_string(&(child->name), len - i, data + i);

		int n = nbt_unpack_node_data(child, i, len, data);
		valid_nbt = n > 0;
		i += n;

		list_append(root->data.children, sizeof(struct node *), &child);
	}

	if (!valid_nbt) {
		return -1;
	}
	return i + 1;
}

struct nbt *nbt_unpack(size_t len, const uint8_t *data) {
	size_t i = 0;
	char *root_name = NULL;
	if (data[i] == TAG_Compound) {
		++i;
		i += nbt_read_string(&root_name, len - i, data + i);
	}
	struct nbt *root = nbt_new(root_name);
	if (nbt_unpack_node(root, i, len, data) < 0) {
		/* TODO free tree */
		return NULL;
	}
	return root;
}

/* TODO: Before implementing this, make a function nbt_traverse() that
 *       walks through the tree and returns the total size it would be
 *       in bytes so a proper buffer can be allocated.
 *
 *       If the buffer length is guaranteed to be big enough and the
 *       write functions behave correctly, they shouldn't have to constantly
 *       checking for overflow like the nbt_read_* functions.
 */
size_t nbt_pack(struct nbt *root, uint8_t **data);
