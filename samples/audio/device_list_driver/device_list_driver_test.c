/* The audio Device list after the driver setting changes.
 *
 * Picking a driver in the menu writes the setting and rebuilds the
 * page; it does not reinitialise audio. The running driver stays what
 * it was until a restart, and the device list - rebuilt each time the
 * Device screen opens - was asked of the running driver, so it showed
 * the old driver's devices under the new driver's name. The Device
 * setting is applied to the configured driver, so its list has to
 * come from the configured driver: from the running instance when
 * that is the one configured, from the configured driver with no
 * context otherwise.
 *
 * With the list now able to come from a driver other than the one
 * running, the driver that built it is recorded with it and is the
 * one that frees it. Every device_list_free in the tree is a plain
 * string_list_free today, so a mismatch would cost nothing today; the
 * record is so that stays true when one is not.
 *
 * Includes audio/audio_driver.c so the shipping selection runs. Two
 * real drivers from the table: alsa, enumerating on this box through
 * libasound's hints, and null, which enumerates nothing. Under ASan so
 * a list freed by the wrong driver, or twice, or not at all, shows. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "../../../audio/audio_driver.c"

extern microphone_driver_t microphone_alsa;

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

static void configure(const char *ident)
{
   settings_t *settings = config_get_ptr();
   strlcpy(settings->arrays.audio_driver, ident,
         sizeof(settings->arrays.audio_driver));
}

int main(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   void *list               = NULL;
   unsigned new_rate        = 0;
   void *alsa_ctx;

   /* alsa running on the null PCM, as after a normal init. */
   alsa_ctx = audio_alsa.init("null", 48000, 64, 1024, &new_rate);
   if (!alsa_ctx)
   {
      printf("SKIP: null PCM unavailable\n");
      return 0;
   }
   st->current_audio      = &audio_alsa;
   st->context_audio_data = alsa_ctx;

   /* 1. Configured driver is the running one: enumerate through it. */
   configure("alsa");
   audio_driver_refresh_devices_list();
   CHECK(st->devices_list_driver == &audio_alsa,
         "alsa configured and running: list not built by alsa");
   CHECK(audio_driver_get_devices_list(&list) && list,
         "alsa configured and running: no list");

   /* 2. The user picks null in the menu; alsa is still running. The
    *    list must now be null's - which enumerates nothing - not
    *    alsa's devices under null's name. The alsa-built list is freed
    *    by alsa. */
   configure("null");
   audio_driver_refresh_devices_list();
   CHECK(st->devices_list_driver != &audio_alsa,
         "null configured, alsa running: list still attributed to alsa");
   list = NULL;
   CHECK(!audio_driver_get_devices_list(&list) || !list,
         "null configured, alsa running: a device list came from the running driver");

   /* 3. Back to alsa: its list again, from the running instance. */
   configure("alsa");
   audio_driver_refresh_devices_list();
   CHECK(st->devices_list_driver == &audio_alsa,
         "alsa reconfigured: list not built by alsa");
   list = NULL;
   CHECK(audio_driver_get_devices_list(&list) && list,
         "alsa reconfigured: no list");

   /* 4. Nothing running at all, alsa configured: enumerate with no
    *    context, which enumeration allows. */
   audio_alsa.free(alsa_ctx);
   st->current_audio      = NULL;
   st->context_audio_data = NULL;
   audio_driver_refresh_devices_list();
   CHECK(st->devices_list_driver == &audio_alsa,
         "alsa configured, nothing running: list not built by alsa");

   /* 5. Release: by the builder, once. */
   audio_driver_free_devices_list();
   CHECK(!st->devices_list && !st->devices_list_driver,
         "free left the list or its builder behind");
   CHECK(!audio_driver_free_devices_list(),
         "a second free reported work to do");

   /* 6. The microphone list, the same way. Nothing is running - the
    *    usual state, a microphone driver opening only when a core asks
    *    for one - and the list must still come from the configured
    *    driver: alsa enumerates its inputs with no context, null lists
    *    nothing. It used to be built only when a driver had opened. */
   {
      microphone_driver_state_t *mic_st = &mic_driver_st;
      settings_t *settings = config_get_ptr();
      memset(mic_st, 0, sizeof(*mic_st));

      strlcpy(settings->arrays.microphone_driver, "alsa",
            sizeof(settings->arrays.microphone_driver));
      microphone_driver_refresh_devices_list();
      CHECK(mic_st->devices_list_driver == &microphone_alsa,
            "mic: alsa configured, nothing running: list not built by alsa");

      strlcpy(settings->arrays.microphone_driver, "null",
            sizeof(settings->arrays.microphone_driver));
      microphone_driver_refresh_devices_list();
      CHECK(mic_st->devices_list_driver != &microphone_alsa,
            "mic: null configured: list still attributed to alsa");

      microphone_driver_free_devices_list();
      CHECK(!mic_st->devices_list && !mic_st->devices_list_driver,
            "mic: free left the list or its builder behind");
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("device list: follows the configured driver, freed by its builder\n");
   return 0;
}
