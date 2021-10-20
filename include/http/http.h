/* provides basic HTTP/1.1 functions based on 
 * (https://datatracker.ietf.org/doc/html/rfc2616) */
#ifndef CHOWDER_HTTP_H
#define CHOWDER_HTTP_H

#include "hashmap.h"
#include <stdint.h>
#include <openssl/ssl.h>

enum http_status_code {
	HTTP_STATUS_CONTINUE				= 100,
	HTTP_STATUS_SWITCHING_PROTOCOLS			= 101,
	HTTP_STATUS_OK					= 200,
	HTTP_STATUS_CREATED				= 201,
	HTTP_STATUS_ACCEPTED				= 202,
	HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION	= 203,
	HTTP_STATUS_NO_CONTENT				= 204,
	HTTP_STATUS_RESET_CONTENT			= 205,
	HTTP_STATUS_PARTIAL_CONTENT			= 206,
	HTTP_STATUS_MULTIPLE_CHOICES			= 300,
	HTTP_STATUS_MOVED_PERMANENTLY			= 301,
	HTTP_STATUS_FOUND				= 302,
	HTTP_STATUS_SEE_OTHER				= 303,
	HTTP_STATUS_USE_PROXY				= 305,
	HTTP_STATUS_TEMPORARY_REDIRECT			= 306,
	HTTP_STATUS_BAD_REQUEST				= 400,
	HTTP_STATUS_UNAUTHORIZED			= 401,
	HTTP_STATUS_PAYMENT_REQUIRED			= 402,
	HTTP_STATUS_FORBIDDEN				= 403,
	HTTP_STATUS_NOT_FOUND				= 404,
	HTTP_STATUS_METHOD_NOT_ALLOWED			= 405,
	HTTP_STATUS_NOT_ACCEPTABLE			= 406,
	HTTP_STATUS_PROXY_AUTH_REQUIRED			= 407,
	HTTP_STATUS_REQUEST_TIMEOUT			= 408,
	HTTP_STATUS_CONFLICT				= 409,
	HTTP_STATUS_GONE				= 410,
	HTTP_STATUS_LENGTH_REQUIRED			= 411,
	HTTP_STATUS_PRECONDITION_FAILED			= 412,
	HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE		= 413,
	HTTP_STATUS_REQUEST_URI_TOO_LARGE		= 414,
	HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE		= 415,
	HTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE	= 416,
	HTTP_STATUS_EXPECTATION_FAILED			= 417,
	HTTP_STATUS_INTERNAL_SERVER_ERROR		= 500,
	HTTP_STATUS_NOT_IMPLEMENTED			= 501,
	HTTP_STATUS_BAD_GATEWAY				= 502,
	HTTP_STATUS_SERVICE_UNAVAILABLE			= 503,
	HTTP_STATUS_GATEWAY_TIMEOUT			= 504,
	HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED		= 505,
};

#define HTTP_METHOD_OPTIONS	"OPTIONS"
#define HTTP_METHOD_GET		"GET"
#define HTTP_METHOD_HEAD	"HEAD"
#define HTTP_METHOD_POST	"POST"
#define HTTP_METHOD_PUT		"PUT"
#define HTTP_METHOD_DELETE	"DELETE"
#define HTTP_METHOD_TRACE	"TRACE"
#define HTTP_METHOD_CONNECT	"CONNECT"

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
	enum http_status_code status_code;
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

void http_request_init(struct http_request *, struct http_uri *);

http_uri_parse_err http_parse_uri(const char *uri, struct http_uri *);
struct http_response http_get(const struct http_uri *, const struct http_message *);
http_err https_send(struct http_ctx *, const struct http_request *, struct http_response *);

void http_uri_free(struct http_uri *);
void http_request_free(struct http_request *);
void http_response_free(struct http_response *);

#endif // CHOWDER_HTTP_H
