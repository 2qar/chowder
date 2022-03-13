#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "window_items.h"

/* FIXME: ignores NBT for now */
bool slot_equal(struct slot *s1, struct slot *s2)
{
	return s1->present == s2->present
		&& s1->item_id == s2->item_id
		&& s1->item_count == s2->item_count;
}

bool slots_equal(size_t slots_len, struct slot *s1, struct slot *s2)
{
	size_t i = 0;
	while (i < slots_len && slot_equal(&s1[i], &s2[i])) {
		++i;
	}
	return i == slots_len;
}

bool window_items_equal(struct window_items *w1, struct window_items *w2)
{
	return w1->window_id == w2->window_id
		&& w1->slot_data_len == w2->slot_data_len
		&& slots_equal(w1->slot_data_len, w1->slot_data, w2->slot_data);
}

int main()
{
	struct window_items window_items;
	window_items.window_id = 12;
	window_items.slot_data_len = 2;
	window_items.slot_data = calloc(2, sizeof(struct slot));
	window_items.slot_data[0].present = true;
	window_items.slot_data[0].item_id = 4;
	window_items.slot_data[0].item_count = 5;
	window_items.slot_data[1].present = true;
	window_items.slot_data[1].item_id = 10;
	window_items.slot_data[1].item_count = 1;

	struct test t = {0};
	test_init(&t, PACKET_FILE_PATH);
	if (t.conn == NULL)
		return 1;

	struct protocol_err r = protocol_write_window_items(t.conn->packet, &window_items);
	if (r.err_type != PROTOCOL_ERR_SUCCESS)
		return 1;
	else if (!conn_write_packet(t.conn))
		return 1;

	test_read_init(&t, PACKET_FILE_PATH);

	if (!conn_packet_read_header(t.conn)) {
		fprintf(stderr, "failed to read header?\n");
		return 1;
	}
	struct window_items *read_window_items = NULL;
	r = protocol_read_window_items(t.conn->packet, &read_window_items);
	bool equal = window_items_equal(&window_items, read_window_items);
	free(read_window_items);
	if (!equal) {
		fprintf(stderr, "read window items differs from actual window items\n");
		return 1;
	}

	printf("%s\n", t.packet_file_path);
	test_cleanup(&t);
	return 0;
}
