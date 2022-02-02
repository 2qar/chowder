#include "nbt.h"

#include <arpa/inet.h>
#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct nbt *nbt_new(enum tag t, char *name)
{
	struct nbt *root = calloc(1, sizeof(struct nbt));
	root->tag = t;
	if (name != NULL) {
		root->name = strdup(name);
	}
	if (t != TAG_Compound) {
		struct nbt *compound_root = calloc(1, sizeof(struct nbt));
		compound_root->tag = TAG_Compound;
		compound_root->data.children = list_new();
		list_append(compound_root->data.children, sizeof(struct nbt *),
			    &root);
		return compound_root;
	} else {
		return root;
	}
}

static void nbt_free_node_data(struct nbt *);

static void nbt_free_list(struct nbt_list *l)
{
	while (!list_empty(l->head)) {
		struct nbt *node = list_remove(l->head);
		nbt_free_node_data(node);
		free(node);
	}
	list_free(l->head);
	free(l);
}

static void nbt_free_node(struct nbt *);

static void nbt_free_node_data(struct nbt *node)
{
	switch (node->tag) {
	case TAG_String:
		free(node->data.string);
		break;
	case TAG_Byte_Array:
	case TAG_Int_Array:
	case TAG_Long_Array:
		free(node->data.array->data.bytes);
		free(node->data.array);
		break;
	case TAG_List:
		nbt_free_list(node->data.list);
		break;
	case TAG_Compound:
		nbt_free_node(node);
		break;
	default:
		break;
	}
}

static void nbt_free_child(struct nbt *child)
{
	nbt_free_node_data(child);
	if (child->tag != TAG_Compound) {
		free(child->name);
	}
	free(child);
}

static void nbt_free_node(struct nbt *root)
{
	free(root->name);
	while (!list_empty(root->data.children)) {
		struct nbt *child = list_remove(root->data.children);
		nbt_free_child(child);
	}
	list_free(root->data.children);
}

void nbt_free(struct nbt *root)
{
	if (root->tag == TAG_Compound) {
		nbt_free_node(root);
		free(root);
	} else {
		nbt_free_child(root);
	}
}

static int nbt_read_bytes(size_t n, void *dest, size_t i, size_t len,
			  const uint8_t *data)
{
	if (n < len && i < len - n) {
		memcpy(dest, data + i, n);
		return n;
	}
	return -1;
}

static int nbt_read_short(int16_t *v, size_t i, size_t len, const uint8_t *data)
{
	int n = nbt_read_bytes(2, v, i, len, data);
	if (n > 0) {
		*v = ntohs(*v);
	}
	return n;
}

static int nbt_read_int(int32_t *v, size_t i, size_t len, const uint8_t *data)
{
	int n = nbt_read_bytes(4, v, i, len, data);
	if (n > 0) {
		*v = ntohl(*v);
	}
	return n;
}

static int nbt_read_long(int64_t *v, size_t i, size_t len, const uint8_t *data)
{
	int n = nbt_read_bytes(8, v, i, len, data);
	if (n > 0) {
		*v = be64toh(*v);
	}
	return n;
}

static size_t nbt_read_string(char **name, size_t len, const uint8_t *data)
{
	size_t i = 0;

	uint16_t name_len;
	int n = nbt_read_short((int16_t *) &name_len, i, len, data);
	if (n > 0) {
		i += n;
		if (name_len > 0) {
			*name = malloc(sizeof(char) * (name_len + 1));
			n = nbt_read_bytes(name_len, *name, i, len, data);
			if (n > 0) {
				i += n;
				(*name)[n] = '\0';
			}
		} else {
			*name = NULL;
		}
	}
	return i;
}

static int nbt_unpack_node_data(struct nbt *, size_t, size_t, const uint8_t *);

static int nbt_read_list(struct nbt_list *l, size_t len, const uint8_t *data)
{
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

static int nbt_read_array(struct nbt_array *a, size_t elem_bytes, size_t len,
			  const uint8_t *data)
{
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

static int nbt_read_int_array(struct nbt_array **array, size_t len,
			      const uint8_t *data)
{
	struct nbt_array *a = malloc(sizeof(struct nbt_array));
	a->type = TAG_Int_Array;
	int n = nbt_read_array(a, 4, len, data);
	for (int32_t i = 0; i < a->len; ++i) {
		a->data.ints[i] = ntohl(a->data.ints[i]);
	}
	*array = a;
	return n;
}

static int nbt_read_long_array(struct nbt_array **array, size_t len,
			       const uint8_t *data)
{
	struct nbt_array *a = malloc(sizeof(struct nbt_array));
	a->type = TAG_Long_Array;
	int n = nbt_read_array(a, 8, len, data);
	for (int32_t i = 0; i < a->len; ++i) {
		a->data.longs[i] = be64toh(a->data.longs[i]);
	}
	*array = a;
	return n;
}

static ssize_t nbt_unpack_node(struct nbt *, size_t, size_t, const uint8_t *);

static int nbt_unpack_node_data(struct nbt *nbt, size_t i, size_t len,
				const uint8_t *data)
{
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
		n = nbt_read_int((int32_t *) &(nbt->data.t_float), i, len,
				 data);
		break;
	case TAG_Double:
		n = nbt_read_long((int64_t *) &(nbt->data.t_double), i, len,
				  data);
		break;
	case TAG_Byte_Array:
		nbt->data.array = malloc(sizeof(struct nbt_array));
		nbt->data.array->type = TAG_Byte_Array;
		n = nbt_read_array(nbt->data.array, 1, len - i, data + i);
		break;
	case TAG_String:
		n = nbt_read_string(&(nbt->data.string), len - i, data + i);
		break;
	case TAG_List:
		nbt->data.list = malloc(sizeof(struct nbt_list));
		n = nbt_read_list(nbt->data.list, len - i, data + i);
		break;
	case TAG_Compound:
		if (i < len) {
			/* FIXME: this isn't very clear */
			n = nbt_unpack_node(nbt, i, len, data) - i;
		} else {
			n = -1;
		}
		break;
	case TAG_Int_Array:
		n = nbt_read_int_array(&(nbt->data.array), len - i, data + i);
		break;
	case TAG_Long_Array:
		n = nbt_read_long_array(&(nbt->data.array), len - i, data + i);
		break;
	default:
		n = -2;
		break;
	}
	return n;
}

static ssize_t nbt_unpack_node(struct nbt *root, size_t i, size_t len,
			       const uint8_t *data)
{
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

		list_append(root->data.children, sizeof(struct list *), &child);
	}

	if (!valid_nbt) {
		return -1;
	}
	return i + 1;
}

size_t nbt_unpack(size_t len, const uint8_t *data, struct nbt **out)
{
	size_t i = 0;
	char *root_name = NULL;
	if (data[i] == TAG_Compound) {
		++i;
		i += nbt_read_string(&root_name, len - i, data + i);
	}
	struct nbt *root = nbt_new(TAG_Compound, root_name);
	ssize_t nbt_len = nbt_unpack_node(root, i, len, data);
	if (nbt_len < 0) {
		nbt_free(root);
		return 0;
	}
	*out = root;
	return nbt_len;
}

static size_t nbt_data_len(struct nbt *);

static size_t nbt_list_len(struct nbt_list *list)
{
	size_t len = 5;
	struct list *l = list->head;
	while (!list_empty(l)) {
		len += nbt_data_len(list_item(l));
		l = list_next(l);
	}
	return len;
}

static size_t nbt_node_len(struct nbt *);

static size_t nbt_data_len(struct nbt *node)
{
	switch (node->tag) {
	case TAG_End:
		return 0;
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
		return 4 + node->data.array->len;
	case TAG_String:
		return 2 + strlen(node->data.string);
	case TAG_List:
		return nbt_list_len(node->data.list);
	case TAG_Compound:
		return nbt_node_len(node);
	case TAG_Int_Array:
		return 4 + node->data.array->len * 4;
	case TAG_Long_Array:
		return 4 + node->data.array->len * 8;
	}
	return 0;
}

static size_t nbt_node_len(struct nbt *n)
{
	size_t len = 0;

	struct list *l = n->data.children;
	while (!list_empty(l)) {
		struct nbt *child = list_item(l);
		len += 3 + strlen(child->name);
		len += nbt_data_len(child);
		l = list_next(l);
	}

	return len + 1;
}

static size_t nbt_len(struct nbt *root)
{
	size_t len = 3;
	if (root->name != NULL) {
		len += strlen(root->name);
	}

	return len + nbt_node_len(root);
}

static size_t nbt_write_short(int16_t s, uint8_t *data)
{
	s = htons(s);
	memcpy(data, &s, sizeof(int16_t));
	return sizeof(int16_t);
}

static size_t nbt_write_int(int32_t i, uint8_t *data)
{
	i = htonl(i);
	memcpy(data, &i, sizeof(int32_t));
	return sizeof(int32_t);
}

static size_t nbt_write_long(int64_t l, uint8_t *data)
{
	l = htobe64(l);
	memcpy(data, &l, sizeof(int64_t));
	return sizeof(int64_t);
}

static size_t nbt_write_float(float f, uint8_t *data)
{
	int32_t i;
	memcpy(&i, &f, 4);
	return nbt_write_int(i, data);
}

static size_t nbt_write_double(double d, uint8_t *data)
{
	int64_t l;
	memcpy(&l, &d, 8);
	return nbt_write_long(l, data);
}

static size_t nbt_write_string(const char *s, uint8_t *data)
{
	if (s == NULL) {
		return nbt_write_short(0, data);
	}

	size_t len = 0;
	size_t s_len = strlen(s);
	len += nbt_write_short(s_len, data);
	data += len;

	while (*s != '\0') {
		*data = *s;
		++data;
		++s;
		++len;
	}
	return len;
}

static size_t nbt_write_byte_array(struct nbt_array *a, uint8_t *data)
{
	size_t len = nbt_write_int(a->len, data);
	for (int32_t i = 0; i < a->len; ++i) {
		data[len] = a->data.bytes[i];
		++len;
	}
	return len;
}

static size_t nbt_pack_node_data(struct nbt *, uint8_t *);

static size_t nbt_write_list(struct nbt_list *list, uint8_t *data)
{
	size_t len = 0;
	data[0] = list->type;
	++len;
	len += nbt_write_int(list_len(list->head), data + 1);

	struct list *l = list->head;
	while (!list_empty(l)) {
		len += nbt_pack_node_data(list_item(l), data + len);
		l = list_next(l);
	}
	return len;
}

static size_t nbt_write_int_array(struct nbt_array *a, uint8_t *data)
{
	size_t len = nbt_write_int(a->len, data);
	for (int32_t i = 0; i < a->len; ++i) {
		len += nbt_write_int(a->data.ints[i], data + len);
	}
	return len;
}

static size_t nbt_write_long_array(struct nbt_array *a, uint8_t *data)
{
	size_t len = nbt_write_int(a->len, data);
	for (int32_t i = 0; i < a->len; ++i) {
		len += nbt_write_long(a->data.longs[i], data + len);
	}
	return len;
}

static size_t nbt_pack_node(struct nbt *, uint8_t *);

static size_t nbt_pack_node_data(struct nbt *n, uint8_t *data)
{
	switch (n->tag) {
	case TAG_Byte:
		*data = n->data.t_byte;
		return 1;
	case TAG_Short:
		return nbt_write_short(n->data.t_short, data);
	case TAG_Int:
		return nbt_write_int(n->data.t_int, data);
	case TAG_Long:
		return nbt_write_long(n->data.t_long, data);
	case TAG_Float:
		return nbt_write_float(n->data.t_float, data);
	case TAG_Double:
		return nbt_write_double(n->data.t_double, data);
	case TAG_Byte_Array:
		return nbt_write_byte_array(n->data.array, data);
	case TAG_String:
		return nbt_write_string(n->data.string, data);
	case TAG_List:
		return nbt_write_list(n->data.list, data);
	case TAG_Compound:
		return nbt_pack_node(n, data);
	case TAG_Int_Array:
		return nbt_write_int_array(n->data.array, data);
	case TAG_Long_Array:
		return nbt_write_long_array(n->data.array, data);
	default:
		return 0;
	}
}

static size_t nbt_pack_node(struct nbt *n, uint8_t *data)
{
	size_t len = 0;

	struct list *l = n->data.children;
	while (!list_empty(l)) {
		struct nbt *child = list_item(l);
		data[len] = child->tag;
		++len;
		len += nbt_write_string(child->name, data + len);
		len += nbt_pack_node_data(child, data + len);

		l = list_next(l);
	}

	data[len] = TAG_End;
	return len + 1;
}

size_t nbt_pack(struct nbt *root, uint8_t **data)
{
	size_t buf_len;
	size_t len = 0;
	if (root->tag == TAG_Compound) {
		buf_len = nbt_len(root);
		*data = malloc(buf_len);

		(*data)[0] = TAG_Compound;
		++len;
		len += nbt_write_string(root->name, (*data) + len);
		len += nbt_pack_node(root, (*data) + len);
	} else {
		const size_t fake_root_len = 4;
		const size_t root_header_len = 3 + strlen(root->name);
		buf_len = fake_root_len + root_header_len + nbt_data_len(root);
		*data = malloc(buf_len);

		(*data)[len] = TAG_Compound;
		++len;
		len += nbt_write_short(0, (*data) + len);
		(*data)[len] = root->tag;
		++len;

		len += nbt_write_string(root->name, (*data) + len);
		len += nbt_pack_node_data(root, (*data) + len);
		(*data)[len] = 0;
		++len;
	}

	assert(len == buf_len);
	return buf_len;
}

static struct nbt *nbt_tree_search(struct nbt *, enum tag, char *, bool);

static struct nbt *nbt_list_search(struct nbt_list *l, enum tag t, char *name)
{
	assert(l->type == TAG_Compound);
	struct nbt *node = NULL;

	struct list *head = l->head;
	while (!list_empty(head) && node == NULL) {
		struct nbt *item = list_item(head);
		node = nbt_tree_search(item, t, name, true);
		head = list_next(head);
	}

	return node;
}

static struct nbt *nbt_tree_search(struct nbt *root, enum tag t, char *name,
				   bool recurse)
{
	struct nbt *node = NULL;

	struct list *head = root->data.children;
	while (!list_empty(head) && node == NULL) {
		struct nbt *child = list_item(head);
		if (child->tag == t && strcmp(child->name, name) == 0) {
			node = child;
		} else if (child->tag == TAG_Compound && recurse) {
			node = nbt_tree_search(child, t, name, recurse);
		} else if (child->tag == TAG_List
			   && child->data.list->type == TAG_Compound
			   && recurse) {
			node = nbt_list_search(child->data.list, t, name);
		}
		head = list_next(head);
	}

	return node;
}

struct nbt *nbt_get(struct nbt *root, enum tag t, char *name)
{
	return nbt_tree_search(root, t, name, false);
}

bool nbt_get_value(struct nbt *root, enum tag t, char *name, void *out)
{
	struct nbt *nbt = nbt_get(root, t, name);
	if (nbt != NULL) {
		size_t bytes = 0;
		switch (t) {
		case TAG_End:
			/* what the dog doin? */
			break;
		case TAG_Byte:
			bytes = 1;
			break;
		case TAG_Short:
			bytes = 2;
			break;
		case TAG_Int:
		case TAG_Float:
			bytes = 4;
			break;
		case TAG_Long:
		case TAG_Double:
			bytes = 8;
			break;
		case TAG_String:
		case TAG_Byte_Array:
		case TAG_Int_Array:
		case TAG_Long_Array:
		case TAG_List:
		case TAG_Compound:
			bytes = sizeof(void *);
			break;
		}
		if (bytes != 0) {
			memcpy(out, &nbt->data, bytes);
		}
	}
	return nbt != NULL;
}

struct nbt *nbt_find(struct nbt *root, enum tag t, char *name)
{
	return nbt_tree_search(root, t, name, true);
}
