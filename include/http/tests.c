#include "http.h"

#include <assert.h>
#include "hashmap.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

const char *uri_err_str[] = {
	"HTTP_URI_OK",
	"HTTP_URI_BAD_SCHEME",
	"HTTP_URI_PORT_EXPECTED",
	"HTTP_URI_PORT_TOO_BIG",
};

#define EIDX(name) [name * -1] = #name

const char *http_err_str[] = {
	EIDX(HTTP_OK),
	EIDX(HTTP_NO_MEM),
	EIDX(HTTP_HOST_NOT_FOUND),
	EIDX(HTTP_HOST_NOT_SUPPORTED),
	EIDX(HTTP_CONNECTION_REFUSED),
	EIDX(HTTP_NO_NETWORK),
	EIDX(HTTP_TIMED_OUT),
	EIDX(HTTP_BAD_RESPONSE),
	EIDX(HTTP_SSL_ERR),
	EIDX(HTTP_SSL_GENERAL_ERR),
	EIDX(HTTP_ABANDON_HOPE),
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

static void test_http_parse_uri()
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
		{ .host = "example.com", .port = 443, .abs_path = "/" },
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
					i, uri_err_str[expected_errors[i]], uri_err_str[err]);
		}
		if (!uri_equal(&uri, &(expected_uris[i]))) {
			printf("test %zu: expected ", i+1);
			print_uri(&(expected_uris[i]));
			printf(", got ");
			print_uri(&uri);
			putchar('\n');
		}
		http_uri_free(&uri);
	}
}

static void test_https_send_simple(SSL_CTX *ssl_ctx)
{
	char *header_keys[] = {
		"Server",
		"Content-Length",
	};
	const char *header_values[] = {
		"GitHub.com",
		"166",
	};
	size_t header_keys_len = sizeof(header_keys) / sizeof(char *);
	size_t header_values_len = sizeof(header_values) / sizeof(char *);
	assert(header_keys_len == header_values_len);

	struct http_ctx ctx = {0};
	ctx.ssl_ctx = ssl_ctx;
	struct http_uri uri;
	struct http_request request = {0};
	struct http_response response = {0};
	http_parse_uri("https://bigheadgeorge.github.io", &uri);
	http_request_init(&request, &uri);

	http_err err = https_send(&ctx, &request, &response);
	if (err != HTTP_OK) {
		fprintf(stderr, "test_https_send_simple(): expected HTTP_OK, got %d\n", err);
	} else if (response.status_code != HTTP_STATUS_OK) {
		fprintf(stderr, "test_https_send_simple(): expected HTTP_STATUS_OK, got %d\n", response.status_code);
	}
	if (err == HTTP_OK && response.status_code == HTTP_STATUS_OK) {
		char *header_value;
		for (size_t i = 0; i < header_keys_len; ++i) {
			header_value = hashmap_get(response.headers, header_keys[i]);
			if (!header_value) {
				fprintf(stderr, "test_https_send_simple(): expected a value for header \"%s\"\n", header_keys[i]);
			} else if (strcmp(header_value, header_values[i])) {
				fprintf(stderr, "test_https_send_simple(): expected the header \"%s\" to be \"%s\", got \"%s\"\n",
						header_keys[i], header_values[i], header_value);
			}
		}

		FILE *website_file = fopen("test/my_site.html", "r");
		fseek(website_file, 0L, SEEK_END);
		size_t website_file_len = ftell(website_file);
		fseek(website_file, 0L, SEEK_SET);
		char *website_str = calloc(website_file_len + 1, sizeof(char));
		size_t n = fread(website_str, 1, website_file_len, website_file);
		fclose(website_file);
		assert(n == website_file_len);
		if (response.message->body_len != website_file_len) {
			fprintf(stderr, "test_https_send_simple(): expected body length of %zu, got %zu\n",
					website_file_len, response.message->body_len);
		} else if (memcmp(website_str, response.message->body, website_file_len)) {
			fprintf(stderr, "test_https_send_simple(): received message body differs from expected message body\n");
		}
		free(website_str);
	}

	hashmap_remove(request.headers, "User-Agent");
	http_request_free(&request);
	http_response_free(&response);
	return;
}

static const char *http_err_to_string(http_err err)
{
	return http_err_str[err * -1];
}

static void test_https_send_errors(SSL_CTX *ssl_ctx)
{
	char *uri_strings[] = {
		"https://thiswebsiteisafakewebsiteforreal.com",
		"https://ogdog.live",
	};
	http_err expected_errors[] = {
		HTTP_HOST_NOT_FOUND,
		HTTP_CONNECTION_REFUSED,
	};
	size_t uri_strings_len = sizeof(uri_strings) / sizeof(char *);
	size_t expected_errors_len = sizeof(expected_errors) / sizeof(http_err);
	assert(uri_strings_len == expected_errors_len);

	struct http_ctx ctx = {0};
	ctx.ssl_ctx = ssl_ctx;
	struct http_uri uri;
	struct http_request request = {0};
	struct http_response response = {0};
	http_request_init(&request, &uri);

	http_err err;
	for (size_t i = 0; i < uri_strings_len; ++i) {
		http_parse_uri(uri_strings[i], &uri);
		err = https_send(&ctx, &request, &response);
		if (err != expected_errors[i]) {
			fprintf(stderr, "test_https_send_errors(): expected %s, got %s\n",
					http_err_to_string(expected_errors[i]), http_err_to_string(err));
		}
		http_uri_free(&uri);
	}

	hashmap_remove(request.headers, "User-Agent");
	request.uri = NULL;
	http_request_free(&request);
	http_response_free(&response);
}

int main()
{
	test_http_parse_uri();
	SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_method());
	test_https_send_simple(ssl_ctx);
	test_https_send_errors(ssl_ctx);
	SSL_CTX_free(ssl_ctx);
}
