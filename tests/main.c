#include <search.h>

#include "read_region.h"
#include "parse_blocks.h"
#include "write_blockstates.h"

int main() {
	test_write_blockstates();

	test_parse_blocks();

	read_and_verify_region();
}
