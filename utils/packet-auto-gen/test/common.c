#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

void test_init(struct test *t, char *packet_file_path)
{
	t->packet_file_path = packet_file_path;
	t->packet_fd = open(t->packet_file_path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	if (t->packet_fd < 0) {
		perror("open");
		return;
	}

	t->conn = calloc(1, sizeof(struct conn));
	t->conn->sfd = t->packet_fd;
	t->conn->packet = malloc(sizeof(struct packet));
	packet_init(t->conn->packet);
}

void test_read_init(struct test *t, char *packet_file_path)
{
	close(t->packet_fd);
	t->packet_fd = open(packet_file_path, O_RDONLY);
	t->conn->sfd = t->packet_fd;
}

void test_cleanup(struct test *t)
{
	close(t->packet_fd);
	if (t->conn != NULL) {
		packet_free(t->conn->packet);
		free(t->conn);
	}
}
