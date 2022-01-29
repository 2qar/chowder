#include "blocks.h"
#include "read_region.h"
#include "write_blockstate.h"

#include <assert.h>

int main()
{
	struct hashmap *hm = create_block_table("../gamedata/blocks.json");
	assert(hm != NULL);
	test_read_region(hm);
	hashmap_free(hm, true, free);
	test_write_blockstate_at();
}
