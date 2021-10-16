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
	char *host_begin = scheme_end + 3;
	char *host_end = strpbrk(host_begin, ":/");
	if (!host_end) {
		host = strdup(host_begin);
	} else {
		size_t host_len = host_end - host_begin + 1;
		host = calloc(host_len, sizeof(char));
		snprintf(host, host_len, "%s", host_begin);
	}

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
	if (port_len) {
		abs_path_begin += 1 + port_len;
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

// FIXME: name sux
// FIXME: errors!!!!!
static int connect_to_resource(const struct http_uri *uri)
{
	struct addrinfo hints = {0};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *res;
	int err = getaddrinfo(uri->host, "www", &hints, &res);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(err));
		return -1;
	} else if (res == NULL) {
		fprintf(stderr, "no results found by getaddrinfo()\n");
		return -1;
	}
	int sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}
	struct sockaddr_in *addr = (struct sockaddr_in *) res->ai_addr;
	addr->sin_port = htons(uri->port);
	if (connect(sfd, res->ai_addr, res->ai_addrlen) == -1) {
		perror("connect");
		return -1;
	}
	freeaddrinfo(res);
	return sfd;
}

static size_t make_request_string(const struct http_request *request, char **request_str)
{
	size_t request_str_len = strlen("GET") + 2 + strlen("HTTP/1.1\r\n");
	request_str_len += strlen(request->request_uri->abs_path);
	request_str_len += strlen("Host: \r\n") + strlen(request->request_uri->host);
	struct node *headers = hashmap_entries(request->request_headers);
	struct node *h = headers;
	while (!list_empty(h)) {
		struct bucket_entry *b = list_item(h);
		request_str_len += strlen(b->key) + strlen(": ") + strlen(b->value) + strlen("\r\n");
		h = list_next(h);
	}
	request_str_len += strlen("\r\n");
	if (request->request_message) {
		 request_str_len += request->request_message->message_length;
	}

	char *buf = calloc(request_str_len + 1, sizeof(char));
	if (!buf) {
		return 0;
	}
	*request_str = buf;

	size_t n = sprintf(buf, "GET %s HTTP/1.1\r\n", request->request_uri->abs_path);
	n += sprintf(buf + n, "Host: %s\r\n", request->request_uri->host);
	h = headers;
	while (!list_empty(h)) {
		struct bucket_entry *b = list_item(h);
		n += sprintf(buf + n, "%s: %s\r\n", b->key, (char *) b->value);
		h = list_next(h);
	}
	list_free_nodes(headers);
	n += sprintf(buf + n, "\r\n");
	if (request->request_message && request->request_message->message_length > 0) {
		memcpy(buf + n, request->request_message->message_body, request->request_message->message_length);
		n += request->request_message->message_length;
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
// FIXME: errors!!!
static struct http_response *parse_response_string(char *response_str)
{
	int version_major, version_minor, status_code;
	int n = sscanf(response_str, "HTTP/%d.%d %d ", &version_major, &version_minor, &status_code);
	if (n != 3) {
		return NULL;
	}
	size_t reason_start = strlen("HTTP/.  ") + digits(version_major)
		+ digits(version_minor) + digits(status_code);
	size_t reason_end = reason_start;
	while (response_str[reason_end] != '\0' && (isalpha(response_str[reason_end])
				|| ispunct(response_str[reason_end]))) {
		++reason_end;
	}
	if (reason_end == reason_start || response_str[reason_end] == '\0') {
		return NULL;
	}
	char *header = response_str + reason_end;
	if (header[1] == '\0') {
		return NULL;
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
		return NULL;
	}
	struct hashmap *headers = hashmap_new(headers_len);
	header = response_str + reason_end;
	while (strncmp(header, "\r\n", 2)) {
		char *value_start = index(header, ':') + 1;
		char *value_end = index(value_start, '\r');
		hashmap_add(headers, strndup(header, value_start - header),
				strndup(value_start, value_end - value_start));
		header = strstr(header, "\r\n") + 2;
	}
	char *response_body_start = header + 2;
	size_t response_body_len = strlen(response_body_start);
	char *response_body = NULL;
	if (response_body_len != 0) {
		response_body = calloc(response_body_len + 1, sizeof(char));
		memcpy(response_body, response_body_start, response_body_len);
	}
	size_t reason_len = reason_end - reason_start;
	char *reason = calloc(reason_len + 1, sizeof(char));
	memcpy(reason, &response_str[reason_start], reason_len);

	struct http_response *response = malloc(sizeof(struct http_response));
	response->response_status_code = status_code;
	response->response_reason = reason;
	response->response_headers = headers;
	response->response_message = malloc(sizeof(struct http_message));
	// FIXME: read that todo above about the headers
	response->response_message->message_headers = NULL;
	response->response_message->message_length = response_body_len;
	response->response_message->message_body = response_body;
	return response;
}

// FIXME: errors!!!
struct http_response *https_get(SSL_CTX *ssl_ctx, const struct http_request *request)
{
	int sfd = connect_to_resource(request->request_uri);
	SSL *ssl = SSL_new(ssl_ctx);
	if (ssl == NULL) {
		fprintf(stderr, "SSL_new(): %ld\n", ERR_get_error());
		return NULL;
	}
	if (!SSL_set_fd(ssl, sfd)) {
		fprintf(stderr, "SSL_set_fd(): %ld\n", ERR_get_error());
		return NULL;
	}

	int err = SSL_connect(ssl);
	if (err <= 0) {
		fprintf(stderr, "SSL_connect(): %d\n", SSL_get_error(ssl, err));
		char err_str[256];
		ERR_error_string(ERR_get_error(), err_str);
		printf("error: %s\n", err_str);
		return NULL;
	}

	char *buf;
	size_t buf_len;
	size_t request_str_len = make_request_string(request, &buf);
	if (!request_str_len) {
		return NULL;
	}
	buf_len = request_str_len;
	size_t n = SSL_write(ssl, buf, buf_len);
	if (n <= 0) {
		// FIXME: maybe take an http_ctx arg and attach errors to that?
		return NULL;
	}
	buf[n] = '\0';
	// FIXME: this is always 0? figure out a better way to read the whole
	//        message, dummy
	size_t pending = SSL_pending(ssl);
	if (pending > buf_len) {
		buf_len = pending;
		buf = realloc(buf, buf_len);
	}
	n = SSL_read(ssl, buf, buf_len);
	if (n <= 0) {
		// FIXME: see above
		return NULL;
	}
	SSL_shutdown(ssl);
	close(sfd);
	SSL_free(ssl);

	struct http_response *response = parse_response_string(buf);
	return response;
}
