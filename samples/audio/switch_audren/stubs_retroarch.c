/* The frontend symbols audio/drivers/switch_libnx_audren_audio.c
 * refers to. */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
uint32_t audio_driver_requested_layout(void) { return 0x3; }
unsigned audio_layout_channels(uint32_t layout)
{
   unsigned n = 0;
   while (layout) { n += layout & 1u; layout >>= 1; }
   return n;
}
