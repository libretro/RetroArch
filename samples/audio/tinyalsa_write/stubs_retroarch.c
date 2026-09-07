/* The frontend logging symbols audio/drivers/tinyalsa.c reaches. */
#include <stdio.h>
#include <stdarg.h>

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   printf("ERR: ");
   vprintf(fmt, ap);
   va_end(ap);
}
