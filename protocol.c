#include "protocol.h"

struct protocol_do_err protocol_do_write(protocol_do_func write_func, struct conn *conn, void *packet_data)
{
	struct protocol_do_err err = {0};
	struct protocol_err protocol_err = write_func(conn->packet, packet_data);
	if (protocol_err.err_type != PROTOCOL_ERR_SUCCESS) {
		err.err_type = PROTOCOL_DO_ERR_PROTOCOL;
		err.protocol_err = protocol_err;
		return err;
	}
	printf("INFO: wrote 0x%02x\n", conn->packet->packet_id);
	ssize_t write_err = conn_write_packet(conn);
	if (write_err < 0) {
		err.err_type = PROTOCOL_DO_ERR_WRITE;
		err.write_err = write_err;
	}
	return err;
}

struct protocol_do_err protocol_do_read(protocol_do_func read_func, struct conn *conn, void *packet_data)
{
	struct protocol_do_err err = {0};
	int read_err = conn_packet_read_header(conn);
	if (read_err < 0) {
		err.err_type = PROTOCOL_DO_ERR_READ;
		err.read_err = read_err;
		return err;
	}
	struct protocol_err protocol_err = read_func(conn->packet, packet_data);
	if (protocol_err.err_type != PROTOCOL_ERR_SUCCESS) {
		err.err_type = PROTOCOL_DO_ERR_PROTOCOL;
		err.protocol_err = protocol_err;
		return err;
	}
	return err;
}
