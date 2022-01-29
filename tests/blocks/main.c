#include "blocks.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
	struct hashmap *blocks =
	    create_block_table("../../gamedata/blocks.json");
	assert(blocks != NULL);

	FILE *states = fopen("states.txt", "r");
	assert(states != NULL);
	char name[256];
	int64_t id;
	while (fscanf(states, "%s %ld\n", name, &id) != EOF) {
		int64_t *table_id = hashmap_get(blocks, name);
		assert(table_id != NULL);
		assert(*table_id == id);
	}
	fclose(states);
	hashmap_free(blocks, true, free);
}
