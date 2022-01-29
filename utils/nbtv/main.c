/* nbtv pretty-prints binary NBT blobs. */
#include "list.h"
#include "nbt.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <zlib.h>

#define INDENT_SPACES 4
#define READ_BUF_SIZE 1024

const char *tag_names[] = { "TAG_End",	     "TAG_Byte",       "TAG_Short",
			    "TAG_Int",	     "TAG_Long",       "TAG_Float",
			    "TAG_Double",    "TAG_Byte_Array", "TAG_String",
			    "TAG_List",	     "TAG_Compound",   "TAG_Int_Array",
			    "TAG_Long_Array" };

void put_indent(int level)
{
	for (int i = 0; i < level * INDENT_SPACES; ++i) {
		putchar(' ');
	}
}

void print_array(struct nbt_array *a, int indent)
{
	printf("%d entries\n", a->len);
	for (int32_t i = 0; i < a->len; ++i) {
		put_indent(indent + 1);
		printf("%d: ", i);
		switch (a->type) {
		case TAG_Byte_Array:
			printf("%d\n", a->data.bytes[i]);
			break;
		case TAG_Int_Array:
			printf("%d\n", a->data.ints[i]);
			break;
		case TAG_Long_Array:
			printf("%ld\n", a->data.longs[i]);
			break;
		default:
			break;
		}
	}
}

void print_node_data(struct nbt *, int indent);
void print_tree_rec(struct nbt *root, int indent);

void print_list(struct nbt_list *nbt_list, int indent)
{
	struct list *list = nbt_list->head;
	int idx = 0;
	while (!list_empty(list)) {
		struct nbt *nbt = list_item(list);
		put_indent(indent + 1);
		printf("%d: ", idx);
		if (nbt_list->type == TAG_Compound) {
			print_tree_rec(nbt, indent + 1);
		} else {
			print_node_data(nbt, indent + 1);
		}
		list = list_next(list);
		++idx;
	}
}

void print_node_data(struct nbt *node, int indent)
{
	switch (node->tag) {
	case TAG_Byte:
		printf("%d\n", node->data.t_byte);
		break;
	case TAG_Short:
		printf("%d\n", node->data.t_short);
		break;
	case TAG_Int:
		printf("%d\n", node->data.t_int);
		break;
	case TAG_Long:
		printf("%ld\n", node->data.t_long);
		break;
	case TAG_Float:
		printf("%f\n", node->data.t_float);
		break;
	case TAG_Double:
		printf("%f\n", node->data.t_double);
		break;
	case TAG_String:
		printf("'%s'\n", node->data.string);
		break;
	case TAG_Byte_Array:
	case TAG_Int_Array:
	case TAG_Long_Array:
		print_array(node->data.array, indent);
		break;
	case TAG_List:
		printf("%d %s entries\n", list_len(node->data.list->head),
		       tag_names[node->data.list->type]);
		print_list(node->data.list, indent);
		break;
	default:
		printf("idk how to handle this\n");
		break;
	}
}

void print_node_name(struct nbt *nbt)
{
	printf("%s(", tag_names[nbt->tag]);
	if (nbt->name != NULL) {
		printf("'%s'", nbt->name);
	} else {
		printf("None");
	}
	printf("): ");
}

void print_tree_rec(struct nbt *root, int indent)
{
	struct list *children = root->data.children;
	print_node_name(root);
	printf("%d entries\n", list_len(root->data.children));
	put_indent(indent);
	printf("{\n");

	while (!list_empty(children)) {
		struct nbt *nbt = list_item(children);
		if (nbt->tag == TAG_Compound) {
			put_indent(indent + 1);
			print_tree_rec(nbt, indent + 1);
		} else {
			put_indent(indent + 1);
			print_node_name(nbt);
			print_node_data(nbt, indent + 1);
		}
		children = list_next(children);
	}

	put_indent(indent);
	printf("}\n");
}

void print_tree(struct nbt *root)
{
	print_tree_rec(root, 0);
}

void write_tree(struct nbt *root, char *filename)
{
	uint8_t *data = NULL;
	size_t len = nbt_pack(root, &data);
	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		perror("nbtv: fopen: ");
		exit(EXIT_FAILURE);
	}
	size_t n = fwrite(data, 1, len, f);
	printf("wrote %ld bytes to '%s'\n", n, filename);
	fclose(f);
	free(data);
}

enum tag tag_name_to_number(char *name)
{
	int names = sizeof(tag_names) / sizeof(char *);
	int i = 0;
	enum tag t = 0;
	while (i < names && t == 0) {
		if (strcmp(tag_names[i], name) == 0) {
			t = i;
		}
		++i;
	}
	return t;
}

int read_file(int orig_fd, uint8_t **out, size_t *out_len)
{
	int fd = dup(orig_fd);
	gzFile file = gzdopen(fd, "r");
	if (file == NULL) {
		close(fd);
		fprintf(stderr, "nbtv: gzdopen: %s", strerror(errno));
		return -1;
	}

	uint8_t *buf = malloc(READ_BUF_SIZE);
	if (buf == NULL) {
		perror("nbtv: error reading file: malloc");
		return -1;
	}
	void *new_buf = buf;
	unsigned buf_len = READ_BUF_SIZE;
	unsigned off = 0;
	int bytes_read;
	while (new_buf != NULL
	       && (bytes_read = gzread(file, buf + off, READ_BUF_SIZE))
		      == READ_BUF_SIZE) {
		buf_len += READ_BUF_SIZE;
		void *new_buf = realloc(buf, buf_len);
		if (new_buf != NULL) {
			buf = new_buf;
			off += READ_BUF_SIZE;
		}
	}
	gzclose(file);
	if (new_buf == NULL) {
		free(buf);
		perror("nbtv: failed reading file: realloc");
		return -1;
	} else if (bytes_read == -1) {
		free(buf);
		int err;
		const char *err_msg = gzerror(file, &err);
		fprintf(stderr, "nbtv: error reading file: zlib: %s\n",
			err_msg);
		return -1;
	}

	*out = buf;
	*out_len = off + bytes_read;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: nbtv [FILE]\n");
		exit(EXIT_FAILURE);
	}

	char *save_filename = NULL;
	if (argc >= 3) {
		save_filename = argv[2];
	}

	int nbt_fd;
	if (!strcmp(argv[1], "-")) {
		nbt_fd = 0;
	} else {
		nbt_fd = open(argv[1], O_RDONLY);
		if (nbt_fd == -1) {
			fprintf(stderr, "nbtv: failed to open \"%s\": %s\n",
				argv[1], strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	uint8_t *buf;
	size_t buf_len;
	if (read_file(nbt_fd, &buf, &buf_len) < 0) {
		exit(EXIT_FAILURE);
	}
	if (nbt_fd != 0) {
		close(nbt_fd);
	}

	struct nbt *root;
	nbt_unpack(buf_len, buf, &root);
	free(buf);
	if (root == NULL) {
		fprintf(stderr, "nbtv: invalid NBT\n");
		exit(EXIT_FAILURE);
	} else if (save_filename != NULL && argc == 5) {
		enum tag t = tag_name_to_number(argv[3]);
		if (t == TAG_End) {
			fprintf(stderr, "nbtv: invalid tag '%s'\n", argv[3]);
			exit(EXIT_FAILURE);
		}
		struct nbt *node = nbt_find(root, t, argv[4]);
		if (node == NULL) {
			fprintf(stderr, "nbtv: couldn't find tag '%s'\n",
				argv[4]);
			exit(EXIT_FAILURE);
		}
		write_tree(node, save_filename);
		nbt_free(root);
	} else if (save_filename != NULL) {
		write_tree(root, save_filename);
		nbt_free(root);
	} else {
		print_tree(root);
		nbt_free(root);
	}

	exit(EXIT_SUCCESS);
}
