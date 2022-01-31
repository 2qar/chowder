#include "nbt_extra.h"

#include "nbt.h"

#include <zlib.h>

#define READ_BUF_SIZE 1024

/* FIXME: the errors suck!!!! */
static int read_file(int orig_fd, uint8_t **out, size_t *out_len)
{
	int fd = dup(orig_fd);
	gzFile file = gzdopen(fd, "r");
	if (file == NULL) {
		close(fd);
		return -1;
	}

	uint8_t *buf = malloc(READ_BUF_SIZE);
	if (buf == NULL) {
		return -1;
	}
	void *new_buf = buf;
	unsigned buf_len = READ_BUF_SIZE;
	unsigned off = 0;
	int bytes_read;
	while (new_buf != NULL
	       && (bytes_read = gzread(file, buf + off, READ_BUF_SIZE))
		      == READ_BUF_SIZE) {
		buf_len += READ_BUF_SIZE;
		void *new_buf = realloc(buf, buf_len);
		if (new_buf != NULL) {
			buf = new_buf;
			off += READ_BUF_SIZE;
		}
	}
	gzclose(file);
	if (new_buf == NULL) {
		free(buf);
		return -1;
	} else if (bytes_read == -1) {
		free(buf);
		return -1;
	}

	*out = buf;
	*out_len = off + bytes_read;
	return 0;
}

int nbt_unpack_file(int fd, struct nbt **out)
{
	size_t bytes_read;
	uint8_t *buf;
	int err = read_file(fd, &buf, &bytes_read);
	if (err != 0) {
		return err;
	}

	struct nbt *nbt;
	size_t bytes_parsed = nbt_unpack(bytes_read, buf, &nbt);
	if (bytes_parsed == 0) {
		return -1;
	}

	*out = nbt;
	return 0;
}
