#ifndef CHOWDER_BLOCK
#define CHOWDER_BLOCK

#include "hashmap.h"

#ifdef BLOCK_NAMES
extern size_t block_names_len;
extern char **block_names;

void free_block_names();
#endif

struct hashmap *create_block_table(char *block_json_path);

#endif
