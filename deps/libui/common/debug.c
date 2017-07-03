// 13 may 2016
#include "../ui.h"
#include "uipriv.h"

void _implbug(const char *file, const char *line, const char *func, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	realbug(file, line, func, "POSSIBLE IMPLEMENTATION BUG; CONTACT ANDLABS:\n", format, ap);
	va_end(ap);
}

void _userbug(const char *file, const char *line, const char *func, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	realbug(file, line, func, "You have a bug: ", format, ap);
	va_end(ap);
}
