/* Stubs for what the MCP server takes from the frontend besides the
 * command table, which the test provides itself */
#include <stdio.h>
#include <stdarg.h>

#include "../../../verbosity.h"

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}
