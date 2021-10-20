/* provides basic HTTP/1.1 functions based on 
 * (https://datatracker.ietf.org/doc/html/rfc2616) */
#ifndef CHOWDER_HTTP_H
#define CHOWDER_HTTP_H

#include "hashmap.h"
#include <stdint.h>
#include <openssl/ssl.h>

struct http_uri {
	char *host;
	uint16_t port;
	char *abs_path;
};

typedef enum {
	HTTP_URI_OK,
	HTTP_URI_BAD_SCHEME,
	HTTP_URI_PORT_EXPECTED,
	HTTP_URI_PORT_TOO_BIG,
} http_uri_parse_err;

typedef enum {
	HTTP_OK				= 0,
	HTTP_NO_MEM			= -1,
	HTTP_HOST_NOT_FOUND		= -2,
	HTTP_HOST_NOT_SUPPORTED		= -3,
	HTTP_CONNECTION_REFUSED		= -4,
	HTTP_NO_NETWORK			= -5,
	HTTP_TIMED_OUT			= -6,
	HTTP_BAD_RESPONSE		= -7,	// the received response wasn't a valid
						// HTTP/1.1 response
	HTTP_SSL_ERR			= -8,	// SSL_get_error(). check ssl_errno
	HTTP_SSL_GENERAL_ERR		= -9,	// ERR_get_error(). check err_errno
	HTTP_ABANDON_HOPE		= -100, // abandon all hope. i fucked something up
} http_err;

struct http_message {
	struct hashmap *headers;
	size_t body_len;
	char *body;
};

struct http_request {
	struct http_uri *uri;
	char *method;
	struct hashmap *headers;
	struct http_message *message;
};

struct http_response {
	unsigned status_code;
	char *reason;
	struct hashmap *headers;
	struct http_message *message;
};

struct http_ctx {
	SSL_CTX *ssl_ctx;
	int ssl_errno;
	char *ssl_err_func_name;
	unsigned long err_errno;
};

http_uri_parse_err http_parse_uri(const char *uri, struct http_uri *);
struct http_response http_get(const struct http_uri *, const struct http_message *);
http_err https_get(struct http_ctx *, const struct http_request *, struct http_response *);

#endif // CHOWDER_HTTP_H
