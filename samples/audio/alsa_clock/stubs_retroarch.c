#include "../../../audio/audio_driver.h"
/* The frontend logging entry points audio/drivers/alsa.c and
 * audio/common/alsa.c reach for. Signatures copied from verbosity.h
 * rather than guessed; everything goes to stdout so a failing
 * scenario's driver-side complaints are visible in the test log. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../../../configuration.h"
#include "../../../defaults.h"

/* audio/common/alsa.c reads one field - audio_format_negotiation -
 * during hw-params setup; zero is AUTO, which lets the null PCM pick
 * its own sample format. */
settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

/* The harness reads the driver's clock estimate out of what it logs,
 * so that what is asserted is the number a user would see and not a
 * field reached into behind the driver's back. */
char harness_log[8192];

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   char    line[1024];
   va_start(ap, fmt);
   vsnprintf(line, sizeof(line), fmt, ap);
   va_end(ap);
   fputs(line, stdout);
   if (strlen(harness_log) + strlen(line) + 1 < sizeof(harness_log))
      strcat(harness_log, line);
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

/* The layout the driver under test asks the frontend for: stereo. */
uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }

/* The platform's audio defaults: audio_driver_device_block_frames()
 * reads the device's transfer granularity from here, and no platform
 * in a harness reports one. */
struct defaults g_defaults;
