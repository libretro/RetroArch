/* The frontend symbols audio/drivers/alsa.c refers to. */
#include <stdio.h>
#include <stdarg.h>
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"
#include "../../../defaults.h"
static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }
void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [log] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void audio_driver_set_buffer_size(size_t bufsize) { (void)bufsize; }
void audio_driver_set_device_latency(size_t frames) { (void)frames; }

/* The platform's audio defaults: audio_driver_device_block_frames()
 * reads the device's transfer granularity from here, and no platform
 * in a harness reports one. */
struct defaults g_defaults;
