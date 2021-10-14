#include "http.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static bool str_equal(const char *s1, const char *s2)
{
	if ((!s1 || !s2) && s1 != s2) {
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
		return str_equal(u1->host, u2->host)
			&& u1->port == u2->port
			&& str_equal(u1->abs_path, u2->abs_path);
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
	struct http_uri expected[] = {
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

	struct http_uri *uri;
	for (size_t i = 0; i < uri_strings_len; ++i) {
		uri = http_parse_uri(uri_strings[i]);
		if (!uri_equal(uri, &(expected[i]))) {
			printf("test %zu: expected ", i+1);
			print_uri(&(expected[i]));
			printf(", got ");
			print_uri(uri);
			putchar('\n');
		}
	}
}
