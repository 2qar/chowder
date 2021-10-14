#include "http.h"

#include <assert.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>

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
