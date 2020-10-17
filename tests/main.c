#include <search.h>

#include "read_region.h"
#include "parse_blocks.h"
#include "write_blockstate.h"

int main() {
	test_parse_blocks();
	test_read_region();
	test_write_blockstate_at();
}
