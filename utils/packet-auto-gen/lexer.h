#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct token {
	char *start;
	size_t len;
	bool is_sep;
	size_t line;
	size_t col;
	struct token *next;
};

bool token_equals(struct token *, const char *);
struct token *lexer_parse(char *buf);
void free_tokens(struct token *);

#endif // LEXER_H
