/* The frontend symbols camera/drivers/ffmpeg.c refers to. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <retro_atomic.h>

retro_atomic_int_t camera_read_errors;

void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [log] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; if (strstr(fmt, "Failed to read frame:")) retro_atomic_fetch_add_int(&camera_read_errors, 1); va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
