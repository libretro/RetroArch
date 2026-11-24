/* The frontend symbols the OpenAL driver refers to. */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
uint32_t audio_driver_requested_layout(void) { return 0x3; }
void audio_driver_set_device_latency(size_t frames) { (void)frames; }
