#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "set_slot.h"

/* FIXME: ignores NBT for now */
bool slot_equal(struct slot *s1, struct slot *s2)
{
	return s1->present == s2->present
		&& s1->item_id == s2->item_id
		&& s1->item_count == s2->item_count;
}

bool packet_equal(struct set_slot *p1, struct set_slot *p2)
{
	return p1->window_id == p2->window_id
		&& p1->slot == p2->slot
		&& slot_equal(p1->slot_data, p2->slot_data);
}

int main()
{
	struct set_slot s;
	s.window_id = 4;
	s.slot = 5;
	struct slot slot;
	slot.present = true;
	slot.item_id = 10;
	slot.item_count = 1;
	slot.nbt = NULL;
	s.slot_data = &slot;

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_set_slot(t.conn->packet, &s);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	test_read_init(&t, PACKET_FILE_PATH);

	if (!conn_packet_read_header(t.conn)) {
		fprintf(stderr, "failed to read header?\n");
		return 1;
	}
	struct set_slot *read_s = NULL;
	r = protocol_read_set_slot(t.conn->packet, &read_s);
	bool equal = packet_equal(&s, read_s);
	free(read_s);
	if (!equal) {
		fprintf(stderr, "read set_slot differs from actual set_slot\n");
		return 1;
	}

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}
