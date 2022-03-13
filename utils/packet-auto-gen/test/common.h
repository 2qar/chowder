#include <stdio.h>
#include "conn.h"
#include "packet.h"

struct test {
	int packet_fd;
	char *packet_file_path;
	struct conn *conn;
};

void test_init(struct test *, char *packet_file_path);
void test_read_init(struct test *, char *packet_file_path);
void test_cleanup(struct test *);
