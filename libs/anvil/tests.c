#include "region.h"

#include <assert.h>

void test_region()
{
	struct region region = { 0 };
	struct chunk chunk = { .sections_len = 123 };

	region_set_chunk(&region, 5, 5, &chunk);
	assert(region.chunks[5][5]->sections_len == chunk.sections_len);
	assert(region_get_chunk(&region, 5, 5)->sections_len
	       == chunk.sections_len);

	// FIXME: setting a chunk at a negative position in a positive region
	//        should probably be an error guarded by an assert or smth
	region_set_chunk(&region, -2, -2, &chunk);
	assert(region.chunks[1][1]->sections_len == chunk.sections_len);
	assert(region_get_chunk(&region, -2, -2)->sections_len
	       == chunk.sections_len);
}

int main()
{
	test_region();
}
