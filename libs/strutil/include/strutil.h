#ifndef CHOWDER_STRUTIL_H
#define CHOWDER_STRUTIL_H

// it's asprintf(3) without _GNU_SOURCE
int asprintf(char **strp, const char *fmt, ...);

#endif // CHOWDER_STRUTIL_H
