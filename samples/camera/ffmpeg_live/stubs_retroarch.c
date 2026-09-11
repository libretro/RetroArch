/* The frontend symbols camera/drivers/ffmpeg.c refers to. */
#include <stdio.h>
#include <stdarg.h>

void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [log] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
