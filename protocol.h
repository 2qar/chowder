#ifndef CHOWDER_PROTOCOL_H
#define CHOWDER_PROTOCOL_H

#include "protocol_autogen.h"
#include "conn.h"
#include "packet.h"

struct protocol_do_err {
	enum {
		PROTOCOL_DO_ERR_SUCCESS = 0,
		PROTOCOL_DO_ERR_PROTOCOL,
		PROTOCOL_DO_ERR_READ,
		PROTOCOL_DO_ERR_WRITE,
	} err_type;
	union {
		struct protocol_err protocol_err;
		int read_err;
		ssize_t write_err;
	};
};

typedef struct protocol_err (*protocol_do_func)(struct packet *, void *);

struct protocol_do_err protocol_do_write(protocol_do_func, struct conn *, void *packet_data);
struct protocol_do_err protocol_do_read(protocol_do_func, struct conn *, void *packet_data);

#endif // CHOWDER_PROTOCOL_H
