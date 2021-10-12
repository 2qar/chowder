#include "http.h"

#include <ctype.h>
#include <strings.h>
#include <string.h>

struct http_uri *http_parse_uri(const char *uri_str)
{
	char *host = NULL;
	uint16_t port = 80;
	char *abs_path = NULL;

	if (strncmp(uri_str, "http", 4)) {
		return NULL;
	}
	char *scheme_end = index(uri_str, ':');
	if (!scheme_end || strncmp(scheme_end, "://", 3)) {
		return NULL;
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
			return NULL;
		}
		port_len = port_end - (host_end + 1);
		if (port_len > 5) {
			return NULL;
		}
		int n = sscanf(host_end + 1, "%hd", &port);
		if (n != 1) {
			return NULL;
		}
	}
	char *abs_path_begin = host_end;
	if (port_len) {
		abs_path_begin += 1 + port_len;
	}
	if (abs_path_begin && *abs_path_begin != '\0') {
		abs_path = strdup(abs_path_begin);
	}

	struct http_uri *uri = calloc(1, sizeof(struct http_uri));
	uri->host = host;
	uri->port = port;
	uri->abs_path = abs_path;
	return uri;
}
