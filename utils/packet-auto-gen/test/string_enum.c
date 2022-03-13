#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "string_enum.h"

int main()
{
	struct string_enum string_enum = {0};
	string_enum.test = 1;

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	string_enum.level_type = "bad_type";
	struct protocol_err r = protocol_write_string_enum(t.conn->packet, &string_enum);
	if (r.err_type != PROTOCOL_ERR_INPUT ||
			r.input_err.err_type != PROTOCOL_INPUT_ERR_BAD_ENUM_CONSTANT ||
			strcmp(r.input_err.field_name, "level_type")) {
		fprintf(stderr, "expected { .err_type = PROTOCOL_ERR_INPUT, .input_err.err_type = PROTOCOL_INPUT_ERR_BAD_ENUM_CONSTANT, .input_err.field_name = \"level_type\" }\n"
				"     got { .err_type = %d, .input_err.err_type = %d, .input_err.field_name = \"%s\" }\n",
				r.err_type, r.input_err.err_type, r.input_err.field_name);
		return 1;
	}

	ftruncate(t.packet_fd, 0);

	string_enum.level_type = "default";
	r = protocol_write_string_enum(t.conn->packet, &string_enum);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}
