#include <stdio.h>

#include "http.h"

static void test_https_get()
{
	struct http_uri uri;
	http_parse_uri("https://github.com/BigHeadGeorge/", &uri);
	uri.port = 443;
	SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_method());
	struct http_request request = {0};
	request.request_uri = &uri;
	request.request_headers = hashmap_new(20);
	hashmap_add(request.request_headers, "User-Agent", "chowder-http-lib/1.0");
	struct http_response *response = https_get(ssl_ctx, &request);
	printf("%d", response->response_status_code);
	return;
}

int main()
{
	test_https_get();
}
