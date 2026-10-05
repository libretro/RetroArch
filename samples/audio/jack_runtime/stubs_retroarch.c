/* The frontend entry points audio/drivers/jack.c reaches for. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../../configuration.h"
#include "../../../audio/audio_driver.h"

size_t jr_device_latency;

/* An empty audio_device: the driver connects to the server's physical
 * inputs in order. */
settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

audio_driver_state_t *audio_state_get_ptr(void)
{
   static audio_driver_state_t state;
   return &state;
}

uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }

unsigned audio_layout_channels(uint32_t layout)
{
   unsigned n = 0;
   for (; layout; layout &= layout - 1)
      n++;
   return n;
}

void audio_driver_set_device_latency(size_t frames) { jr_device_latency = frames; }

static void jr_log(const char *fmt, va_list ap) { vprintf(fmt, ap); }

void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); jr_log(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); jr_log(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); jr_log(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); jr_log(fmt, ap); va_end(ap); }
