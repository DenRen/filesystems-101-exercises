#include <fs_string.h>
#include <fs_malloc.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <err.h>

char* fs_xasprintf(const char *fmt, ...)
{
	va_list ap;
	int n;

	char buf[256];
	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (n < 0)
		errx(1, "bad format string");

	if ((size_t)n < sizeof(buf))
		return fs_xstrdup(buf);

	char *out = (char*)fs_xmalloc(n + 1);
	int m;
	va_start(ap, fmt);
	m = vsnprintf(out, n + 1, fmt, ap);
	va_end(ap);

	if (m != n)
		errx(1, "vsnprintf() is not deterministic?");

	return out;
}

char* fs_xstrdup(const char *x)
{
	size_t len = strlen(x);
	char *copy = (char*)fs_xmalloc(len + 1);
	memcpy(copy, x, len + 1);
	return copy;
}
