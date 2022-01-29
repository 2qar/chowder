#ifndef CHOWDER_PROTOCOL_H
#define CHOWDER_PROTOCOL_H

#include "conn.h"
#include "packet.h"
#include "protocol_types.h"

struct protocol_do_err {
	enum { PROTOCOL_DO_ERR_SUCCESS = 0,
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

// Helper macros for using protocol_do_write() and protocol_do_read().
#define PROTOCOL_WRITE(PACKET_NAME, CONN, PACKET_DATA)                         \
	protocol_do_write((protocol_write_func) protocol_write_##PACKET_NAME,  \
			  CONN, PACKET_DATA)
#define PROTOCOL_READ(PACKET_NAME, CONN, DEST)                                 \
	protocol_do_read((protocol_read_func) protocol_read_##PACKET_NAME,     \
			 CONN, &(DEST))
// For when PACKET is a value, not a pointer.
#define PROTOCOL_READ_S(PACKET_NAME, CONN, PACKET, ERR_VAR)                    \
	do {                                                                   \
		void *p = &(PACKET);                                           \
		void **p2 = &(p);                                              \
		ERR_VAR = protocol_do_read(                                    \
		    (protocol_read_func) protocol_read_##PACKET_NAME, CONN,    \
		    p2);                                                       \
	} while (0)

struct protocol_do_err protocol_do_write(protocol_write_func, struct conn *,
					 void *packet_data);
struct protocol_do_err protocol_do_read(protocol_read_func, struct conn *,
					void **packet_data_ptr);

#endif // CHOWDER_PROTOCOL_H
