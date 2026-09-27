#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"
#include "../../../defaults.h"
static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }
void RARCH_LOG(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [log] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
static size_t g_buffer, g_latency;
void audio_driver_set_buffer_size(size_t bufsize) { g_buffer = bufsize; }
void audio_driver_set_device_latency(size_t frames) { g_latency = frames; }
uint32_t audio_driver_requested_layout(void) { return 0x3; }

/* The platform's audio defaults: audio_driver_device_block_frames()
 * reads the device's transfer granularity from here, and no platform
 * in a harness reports one. */
struct defaults g_defaults;
