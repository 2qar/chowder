/* nbtv pretty-prints binary NBT blobs. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../../nbt.h"
#include "../../include/linked_list.h"

#define INDENT_SPACES 4

const char *tagNames[] = {"TAG_End", "TAG_Byte", "TAG_Short", "TAG_Int", "TAG_Long", "TAG_Float", "TAG_Double", "TAG_Byte_Array", "TAG_String", "TAG_List", "TAG_Compound", "TAG_Int_Array", "TAG_Long_Array"};

void put_indent(int level)
{
	for (int i = 0; i < level * INDENT_SPACES; ++i) {
		putchar(' ');
	}
}

void print_node_data(struct nbt *node)
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
		/* TODO: print array / list contents */
		case TAG_Byte_Array:
		case TAG_Int_Array:
		case TAG_Long_Array:
			printf("%d entries\n", node->data.array->len);
			break;
		case TAG_List:
			printf("%d %s entries\n", list_len(node->data.list->head), tagNames[node->data.list->type]);
			break;
		default:
			printf("idk how to handle this\n");
			break;
	}
}

void print_tree_rec(struct nbt *root, int indent)
{
	put_indent(indent);
	struct node *children = root->data.children;
	printf("%s('%s'): %d entries\n", tagNames[root->tag], root->name, list_len(root->data.children));
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
			printf("%s('%s'): ", tagNames[nbt->tag], name);
			print_node_data(nbt);
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

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: nbtv [FILE]\n");
		exit(EXIT_FAILURE);
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

	struct nbt *root = nbt_unpack(f_len, buf);
	if (root == NULL) {
		fprintf(stderr, "nbtv: invalid NBT\n");
		exit(EXIT_FAILURE);
	} else {
		print_tree(root);
	}

	free(buf);
	exit(EXIT_SUCCESS);
}
