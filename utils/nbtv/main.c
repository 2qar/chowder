/* nbtv pretty-prints binary NBT blobs. */
#include "list.h"
#include "nbt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define INDENT_SPACES 4

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
		break;
	default:
		printf("idk how to handle this\n");
		break;
	}
}

void print_tree_rec(struct nbt *root, int indent)
{
	put_indent(indent);
	struct list *children = root->data.children;
	printf("%s('%s'): %d entries\n", tag_names[root->tag], root->name,
	       list_len(root->data.children));
	put_indent(indent);
	printf("{\n");

	while (!list_empty(children)) {
		struct nbt *nbt = list_item(children);
		if (nbt->tag == TAG_Compound) {
			print_tree_rec(nbt, indent + 1);
		} else {
			put_indent(indent + 1);
			char *name = "None";
			if (nbt->name != NULL) {
				name = nbt->name;
			}
			printf("%s('%s'): ", tag_names[nbt->tag], name);
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

	FILE *f = fopen(argv[1], "r");
	if (f == NULL) {
		perror("nbtv: fopen: ");
		exit(EXIT_FAILURE);
	}
	fseek(f, 0, SEEK_END);
	size_t f_len = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t *buf = malloc(sizeof(uint8_t) * f_len);
	fread(buf, 1, f_len, f);
	fclose(f);

	struct nbt *root;
	nbt_unpack(f_len, buf, &root);
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
