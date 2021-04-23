#ifndef CHOWDER_BLOCK
#define CHOWDER_BLOCK

#include "include/hashmap.h"

#ifdef BLOCK_NAMES
extern char **block_names;
#endif

struct hashmap *create_block_table(char *block_json_path);

#endif
