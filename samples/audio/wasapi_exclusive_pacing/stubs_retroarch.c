/* The frontend logging entry points audio/drivers/alsa.c and
 * audio/common/alsa.c reach for. Signatures copied from verbosity.h
 * rather than guessed; everything goes to stdout so a failing
 * scenario's driver-side complaints are visible in the test log. */

#include <stdio.h>
#include <stdarg.h>

#include "../../../configuration.h"

/* audio/common/alsa.c reads one field - audio_format_negotiation -
 * during hw-params setup; zero is AUTO, which lets the null PCM pick
 * its own sample format. */
settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

void RARCH_DBG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

/* The device stage the driver reports for the statistics overlay;
 * nothing here reads it. */
void audio_driver_set_device_latency(size_t frames) { (void)frames; }
