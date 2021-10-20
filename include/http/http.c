#include "http.h"

#include <assert.h>
#include <ctype.h>
#include "linked_list.h"
#include <netdb.h>
#include <openssl/err.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define CHUNK_SIZE 4096

http_uri_parse_err http_parse_uri(const char *uri_str, struct http_uri *uri)
{
	char *host = NULL;
	uint16_t port = 80;
	char *abs_path = NULL;

	if (strncmp(uri_str, "http", 4)) {
		return HTTP_URI_BAD_SCHEME;
	}
	char *scheme_end = index(uri_str, ':');
	if (!scheme_end || strncmp(scheme_end, "://", 3)) {
		return HTTP_URI_BAD_SCHEME;
	}
	size_t scheme_len = scheme_end - uri_str;
	if (scheme_len > 5 || (scheme_len == 5 && scheme_end[-1] != 's')) {
		return HTTP_URI_BAD_SCHEME;
	}
	if (scheme_len == 5) {
		port = 443;
	}
	char *host_begin = scheme_end + 3;
	char *host_end = strpbrk(host_begin, ":/");
	size_t port_len = 0;
	if (host_end && *host_end == ':') {
		char *port_end = host_end + 1;
		while (*port_end != '\0' && isdigit(*port_end)) {
			++port_end;
		}
		if (port_end == host_end + 1) {
			return HTTP_URI_PORT_EXPECTED;
		}
		port_len = port_end - (host_end + 1);
		if (port_len > 5) {
			return HTTP_URI_PORT_TOO_BIG;
		}
		assert(sscanf(host_end + 1, "%hu", &port) == 1);
	}
	char *abs_path_begin = host_end;
	if (abs_path_begin && port_len) {
		abs_path_begin += 1 + port_len;
	}
	if (!host_end) {
		host = strdup(host_begin);
	} else {
		size_t host_len = host_end - host_begin + 1;
		host = calloc(host_len, sizeof(char));
		snprintf(host, host_len, "%s", host_begin);
	}
	if (abs_path_begin && *abs_path_begin != '\0') {
		abs_path = strdup(abs_path_begin);
	}

	uri->host = host;
	uri->port = port;
	if (!abs_path) {
		uri->abs_path = strdup("/");
	} else {
		uri->abs_path = abs_path;
	}
	return HTTP_URI_OK;
}

static int connect_to_resource(const struct http_uri *uri)
{
	struct addrinfo hints = {0};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *res;
	int err = getaddrinfo(uri->host, "www", &hints, &res);
	// FIXME: handle EAI_AGAIN
	switch (err) {
		case 0:
			break;
		case EAI_FAMILY:
			return HTTP_HOST_NOT_SUPPORTED;
		case EAI_NONAME:
			return HTTP_HOST_NOT_FOUND;
		case EAI_MEMORY:
			return HTTP_NO_MEM;
		default:
			return HTTP_ABANDON_HOPE;
	}
	if (!res) {
		return HTTP_HOST_NOT_FOUND;
	}
	int sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sfd == -1) {
		switch (errno) {
			case ENOBUFS:
			case ENOMEM:
				return HTTP_NO_MEM;
			default:
				return HTTP_ABANDON_HOPE;
		}
	}
	struct sockaddr_in *addr = (struct sockaddr_in *) res->ai_addr;
	addr->sin_port = htons(uri->port);
	if (connect(sfd, res->ai_addr, res->ai_addrlen) == -1) {
		switch (errno) {
			case ECONNREFUSED:
				return HTTP_CONNECTION_REFUSED;
			case ENETUNREACH:
				return HTTP_NO_NETWORK;
			case ETIMEDOUT:
				return HTTP_TIMED_OUT;
			default:
				return HTTP_ABANDON_HOPE;
		}
	}
	freeaddrinfo(res);
	return sfd;
}

static size_t make_request_string(const struct http_request *request, char **request_str)
{
	size_t request_str_len = strlen("GET") + 2 + strlen("HTTP/1.1\r\n");
	request_str_len += strlen(request->uri->abs_path);
	request_str_len += strlen("Host: \r\n") + strlen(request->uri->host);
	// FIXME: support persistent connections, maybe
	request_str_len += strlen("Connection: close\r\n");
	struct node *headers = hashmap_entries(request->headers);
	struct node *h = headers;
	while (!list_empty(h)) {
		struct bucket_entry *b = list_item(h);
		request_str_len += strlen(b->key) + strlen(": ") + strlen(b->value) + strlen("\r\n");
		h = list_next(h);
	}
	request_str_len += strlen("\r\n");
	if (request->message) {
		 request_str_len += request->message->body_len;
	}

	char *buf = calloc(request_str_len + 1, sizeof(char));
	if (!buf) {
		return 0;
	}
	*request_str = buf;

	size_t n = sprintf(buf, "GET %s HTTP/1.1\r\n", request->uri->abs_path);
	n += sprintf(buf + n, "Host: %s\r\n", request->uri->host);
	n += sprintf(buf + n, "Connection: close\r\n");
	h = headers;
	while (!list_empty(h)) {
		struct bucket_entry *b = list_item(h);
		n += sprintf(buf + n, "%s: %s\r\n", b->key, (char *) b->value);
		h = list_next(h);
	}
	list_free_nodes(headers);
	n += sprintf(buf + n, "\r\n");
	if (request->message && request->message->body_len > 0) {
		memcpy(buf + n, request->message->body, request->message->body_len);
		n += request->message->body_len;
	}
	return n;
}

static size_t digits(int n)
{
	int d = 0;
	while (n > 0) {
		++d;
		n /= 10;
	}
	return d;
}

// FIXME: be more careful while parsing so malformed responses get caught
http_err parse_response_string(char *response_str, struct http_response *response)
{
	int version_major, version_minor, status_code;
	int n = sscanf(response_str, "HTTP/%d.%d %d ", &version_major, &version_minor, &status_code);
	if (n != 3) {
		return HTTP_BAD_RESPONSE;
	}
	size_t reason_start = strlen("HTTP/.  ") + digits(version_major)
		+ digits(version_minor) + digits(status_code);
	size_t reason_end = reason_start;
	while (response_str[reason_end] != '\0' && (isalpha(response_str[reason_end])
				|| ispunct(response_str[reason_end]))) {
		++reason_end;
	}
	if (reason_end == reason_start || response_str[reason_end] == '\0') {
		return HTTP_BAD_RESPONSE;
	}
	char *header = response_str + reason_end;
	if (header[1] == '\0') {
		return HTTP_BAD_RESPONSE;
	}
	header += 2;
	// TODO: The headers probably shouldn't all be lumped together; the HTTP
	//       spec probably groups the headers as general, request/response and
	//       entity for a reason. idk xd
	size_t headers_len = 0;
	while (header != NULL && strncmp(header, "\r\n", 2)) {
		char *value_sep = strpbrk(header, ":\r");
		char *header_end = strstr(header, "\r\n");
		if (value_sep && *value_sep == ':' && value_sep != header && header_end) {
			header = header_end + 2;
			++headers_len;
		} else {
			header = NULL;
		}
	}
	if (!header) {
		return HTTP_BAD_RESPONSE;
	}
	struct hashmap *headers = hashmap_new(headers_len);
	header = response_str + reason_end + 2;
	while (strncmp(header, "\r\n", 2)) {
		char *value_start = index(header, ':') + 1;
		char *value_end = index(value_start, '\r');
		char *key = strndup(header, value_start - 1 - header);
		// FIXME: strip trailing whitespace from values
		hashmap_add(headers, key, strndup(value_start, value_end - value_start));
		// FIXME: hashmap_add() needs a flag or something so it doesn't
		//        duplicate the key string all the time
		free(key);
		header = strstr(header, "\r\n") + 2;
	}
	size_t reason_len = reason_end - reason_start;
	char *reason = calloc(reason_len + 1, sizeof(char));
	memcpy(reason, &response_str[reason_start], reason_len);

	char *response_body_start = header + 2;
	size_t response_body_len = strlen(response_body_start);
	char *response_body = NULL;
	if (response_body_len != 0) {
		response_body = memmove(response_str, response_body_start, response_body_len);
		response_body[response_body_len] = '\0';
	}

	response->status_code = status_code;
	response->reason = reason;
	response->headers = headers;
	response->message = malloc(sizeof(struct http_message));
	// FIXME: read that todo above about the headers
	response->message->headers = NULL;
	response->message->body_len = response_body_len;
	response->message->body = response_body;
	return HTTP_OK;
}

// FIXME: come up with some actual errors for SSL errors
http_err https_get(struct http_ctx *ctx, const struct http_request *request, struct http_response *response)
{
	int sfd = connect_to_resource(request->uri);
	SSL *ssl = SSL_new(ctx->ssl_ctx);
	if (ssl == NULL) {
		ctx->err_errno = ERR_get_error();
		return HTTP_SSL_GENERAL_ERR;
	}
	if (!SSL_set_fd(ssl, sfd)) {
		ctx->err_errno = ERR_get_error();
		return HTTP_SSL_GENERAL_ERR;
	}

	int err = SSL_connect(ssl);
	if (err <= 0) {
		ctx->ssl_errno = SSL_get_error(ssl, err);
		ctx->ssl_err_func_name = "SSL_connect";
		return HTTP_SSL_ERR;
	}

	char *buf;
	size_t buf_len;
	size_t request_str_len = make_request_string(request, &buf);
	if (!request_str_len) {
		return HTTP_NO_MEM;
	}
	buf_len = request_str_len;
	size_t n = SSL_write(ssl, buf, buf_len);
	if (n <= 0) {
		ctx->ssl_errno = SSL_get_error(ssl, n);
		ctx->ssl_err_func_name = "SSL_write";
		return HTTP_SSL_ERR;
	}
	buf_len = CHUNK_SIZE;
	buf = reallocarray(buf, buf_len, sizeof(char));
	size_t i = 0;
	while ((n = SSL_read(ssl, buf + i, (buf_len - i) * sizeof(char))) > 0) {
		i += n;
		if (i == buf_len) {
			buf_len += CHUNK_SIZE;
			buf = reallocarray(buf, buf_len, sizeof(char));
		}
	}
	int ssl_err = SSL_get_error(ssl, n);
	if (ssl_err != SSL_ERROR_NONE && ssl_err != SSL_ERROR_ZERO_RETURN) {
		ctx->ssl_errno = ssl_err;
		ctx->ssl_err_func_name = "SSL_read";
		return HTTP_SSL_ERR;
	}
	if (i == buf_len) {
		buf = reallocarray(buf, buf_len + 1, sizeof(char));
	}
	buf[i] = '\0';
	SSL_shutdown(ssl);
	close(sfd);
	SSL_free(ssl);

	return parse_response_string(buf, response);
}

void http_uri_free(struct http_uri *uri)
{
	free(uri->host);
	free(uri->abs_path);
}

static void http_message_free(struct http_message *message)
{
	if (message) {
		// FIXME: this probably shouldn't be NULL; read the todo in parse_response_string()
		if (message->headers) {
			hashmap_free(message->headers, free);
		}
		free(message->body);
	}
	free(message);
}

void http_request_free(struct http_request *request)
{
	http_uri_free(request->uri);
	free(request->method);
	if (request->headers) {
		hashmap_free(request->headers, free);
	}
	http_message_free(request->message);
}

void http_response_free(struct http_response *response)
{
	free(response->reason);
	if (response->headers) {
		hashmap_free(response->headers, free);
	}
	http_message_free(response->message);
}
