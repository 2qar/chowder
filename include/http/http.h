/* provides basic HTTP/1.1 functions based on 
 * (https://datatracker.ietf.org/doc/html/rfc2616) */
#ifndef CHOWDER_HTTP_H
#define CHOWDER_HTTP_H

#include <stdint.h>

#include <openssl/ssl.h>

struct http_uri {
	char *host;
	uint16_t port;
	char *abs_path;
};

struct http_message {
	struct hashmap *message_headers;
	size_t message_length;
	uint8_t *message_body;
};

struct http_request {
	char *request_method;
	struct hashmap *request_headers;
	struct http_message request_message;
};

struct http_response {
	unsigned response_status_code;
	char *response_reason;
	struct hashmap *response_headers;
};

struct http_uri *http_parse_uri(const char *uri);
struct http_response http_get(const struct http_uri *, const struct http_message *);
struct http_response https_get(SSL_CTX *, const struct http_uri *, const struct http_message *);

#endif // CHOWDER_HTTP_H
