#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool token_equals(struct token *t, const char *s)
{
	if (t == NULL || t->start == NULL)
		return false;
	else
		return !strncmp(t->start, s, t->len);
}

static struct token *token_append(struct token *t, size_t token_len, size_t line, const char *line_start)
{
	t->len = token_len;
	t->line = line;
	t->col = t->start - line_start + 1;
	t->next = calloc(1, sizeof(struct token));
	return t->next;
}

struct token *lexer_parse(char *buf)
{
	struct token *t = calloc(1, sizeof(struct token));
	struct token *tokens = t;
	size_t token_len = 0;
	size_t i = 0;
	size_t line = 1;
	char *line_start = buf;

	while (buf[i] != '\0') {
		if (!t->start && (isalnum(buf[i]) || buf[i] == '-' || buf[i] == '"')) {
			t->start = &buf[i];
			token_len = 0;
		}

		switch (buf[i]) {
			case '\n':
			case '=':
			case '(':
			case ')':
			case '{':
			case '}':
			case ',':
				if (t->start && token_len > 0)
					t = token_append(t, token_len, line, line_start);
				else
					t->start = NULL;

				t->start = &buf[i];
				t->len = 1;
				t->is_sep = true;
				t->line = line;
				t->col = &buf[i] - line_start + 1;
				t->next = calloc(1, sizeof(struct token));
				t = t->next;
				break;
			case ' ':
				if (t->start && token_len > 0)
					t = token_append(t, token_len, line, line_start);
				break;
		}

		if (buf[i] == '\n') {
			line_start = &buf[i+1];
			++line;
		}

		if (t->start)
			++token_len;
		++i;
	}

	return tokens;
}

void free_tokens(struct token *t)
{
	while (t != NULL) {
		struct token *next = t->next;
		free(t);
		t = next;
	}
}
