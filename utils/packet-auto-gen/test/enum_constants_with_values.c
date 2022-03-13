#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "enum_constants_with_values.h"

#define NAMEOF(arg) #arg
#define ASSERT_CONSTANT_EQUALS(c, n) \
	if (c != n) { \
		fprintf(stderr, "expected %s = %d, got %d\n", NAMEOF(c), n, c); \
		return 1; \
	}

int main()
{
	ASSERT_CONSTANT_EQUALS(ENUM_CONSTANTS_WITH_VALUES_DIMENSION_NETHER, -1);
	ASSERT_CONSTANT_EQUALS(ENUM_CONSTANTS_WITH_VALUES_DIMENSION_OVERWORLD, 0);
	ASSERT_CONSTANT_EQUALS(ENUM_CONSTANTS_WITH_VALUES_DIMENSION_END, 5);

	struct enum_constants_with_values enum_struct = {0};
	enum_struct.dimension = ENUM_CONSTANTS_WITH_VALUES_DIMENSION_END;

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_enum_constants_with_values(t.conn->packet, &enum_struct);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}
