#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>

#include "../include/jsmn/jsmn.h"

#include "../blocks.h"

void assert_block_exists(char *key, int value) {
	ENTRY e;
	e.key = key;
	ENTRY *found = hsearch(e, FIND);
	assert(found != NULL);
	assert(found->data != NULL);
	assert(*((int *) found->data) == value);
}

int test_parse_blocks() {
	char *blocks_json = read_blocks_json("../gamedata/blocks.json");
	if (blocks_json == NULL)
		exit(1);

	create_block_table(blocks_json);

	assert_block_exists("minecraft:stone", 1);
	assert_block_exists("minecraft:beehive;facing=south;honey_level=3", 11320);
	assert_block_exists("minecraft:honeycomb_block", 11336);

	free(blocks_json);
	//hdestroy();
	return 0;
}
