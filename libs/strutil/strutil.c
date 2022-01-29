#include "strutil.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int asprintf(char **strp, const char *fmt, ...)
{
	va_list ap;
	va_list len_ap;
	char *str;
	int len;
	int ret;

	va_start(ap, fmt);
	va_copy(len_ap, ap);
	len = vsnprintf(NULL, 0, fmt, len_ap);
	va_end(len_ap);
	if (len < 0) {
		va_end(ap);
		return len;
	}
	str = calloc(len + 1, sizeof(char));
	if (str == NULL) {
		perror("calloc");
		va_end(ap);
		return -1;
	}
	ret = vsnprintf(str, len + 1, fmt, ap);
	*strp = str;
	va_end(ap);
	return ret;
}
