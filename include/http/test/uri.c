#include "http.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

const char *err_str[] = {
	"HTTP_URI_OK",
	"HTTP_URI_BAD_SCHEME",
	"HTTP_URI_PORT_EXPECTED",
	"HTTP_URI_PORT_TOO_BIG",
};

static bool str_equal(const char *s1, const char *s2)
{
	if ((!s1 && !s2) || ((!s1 || !s2) && s1 != s2)) {
		return false;
	} else {
		return !strcmp(s1, s2);
	}
}

static bool uri_equal(const struct http_uri *u1, const struct http_uri *u2)
{
	if ((!u1 && u2 && !u2->host) || (!u2 && u1 && !u1->host)) {
		return true;
	} else if ((!u1 || !u2) && u1 != u2) {
		return false;
	} else {
		return (!u1->host && !u2->host)
			|| (str_equal(u1->host, u2->host)
				&& u1->port == u2->port
				&& str_equal(u1->abs_path, u2->abs_path));
	}
}

static void print_uri(const struct http_uri *uri)
{
	if (!uri || !uri->host) {
		printf("(null)");
	} else {
		printf("{ host=\"%s\", port=%hu, abs_path=\"%s\" }", uri->host, uri->port, uri->abs_path);
	}
}

int main()
{
	char *uri_strings[] = {
		"http://example.com",
		"http://example.com/",
		"http://example.com:42/",
		"https://example.com",
		"httpd://example.com",
		"https://example.com:80129131youshouldn'tmakeithere",
		"http:/example.com",
		"http://www.example.com/",
		"bla",
		"badscheme://example.com",
	};
	size_t uri_strings_len = sizeof(uri_strings) / sizeof(char *);
	http_uri_parse_err expected_errors[] = {
		HTTP_URI_OK,
		HTTP_URI_OK,
		HTTP_URI_OK,
		HTTP_URI_OK,
		HTTP_URI_BAD_SCHEME,
		HTTP_URI_PORT_TOO_BIG,
		HTTP_URI_BAD_SCHEME,
		HTTP_URI_OK,
		HTTP_URI_BAD_SCHEME,
		HTTP_URI_BAD_SCHEME,
	};
	struct http_uri expected_uris[] = {
		{ .host = "example.com", .port = 80, .abs_path = "/" },
		{ .host = "example.com", .port = 80, .abs_path = "/" },
		{ .host = "example.com", .port = 42, .abs_path = "/" },
		{ .host = "example.com", .port = 80, .abs_path = "/" },
		{ 0 },
		{ 0 },
		{ 0 },
		{ .host = "www.example.com", .port = 80, .abs_path = "/" },
		{ 0 },
		{ 0 },
	};
	assert(uri_strings_len == sizeof(expected_errors) / sizeof(http_uri_parse_err));
	assert(uri_strings_len == sizeof(expected_uris) / sizeof(struct http_uri));

	http_uri_parse_err err;
	struct http_uri uri;
	for (size_t i = 0; i < uri_strings_len; ++i) {
		memset(&uri, 0, sizeof(struct http_uri));
		err = http_parse_uri(uri_strings[i], &uri);
		if (err != expected_errors[i]) {
			printf("test %zu: expected %s, got %s\n",
					i, err_str[expected_errors[i]], err_str[err]);
		}
		if (!uri_equal(&uri, &(expected_uris[i]))) {
			printf("test %zu: expected ", i+1);
			print_uri(&(expected_uris[i]));
			printf(", got ");
			print_uri(&uri);
			putchar('\n');
		}
	}
}
