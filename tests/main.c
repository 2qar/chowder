#include <search.h>

#include "read_region.h"
#include "parse_blocks.h"

int main() {
	test_parse_blocks();
	if (test_read_region() > 0)
		return 1;
	return 0;
}
