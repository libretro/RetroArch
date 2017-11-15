// 13 may 2016
#include "uipriv_unix.h"

// LONGTERM don't halt on release builds

void realbug(const char *file, const char *line, const char *func, const char *prefix, const char *format, va_list ap)
{
	char *a, *b;

	a = g_strdup_printf("[libui] %s:%s:%s() %s", file, line, func, prefix);
	b = g_strdup_vprintf(format, ap);
	g_critical("%s%s", a, b);
	G_BREAKPOINT();
}
