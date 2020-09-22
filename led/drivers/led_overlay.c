#include <stdio.h>
#include "../led_driver.h"
#include "../led_defines.h"

#include "../../input/input_overlay.h"

#include "../../configuration.h"
#include "../../retroarch.h"

typedef struct
{
   int setup[MAX_LEDS];
   int map[MAX_LEDS];
} overlayled_t;

/* TODO/FIXME - static globals */
static overlayled_t ledoverlay_curins;
static overlayled_t *ledoverlay_cur = &ledoverlay_curins;

static void overlay_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   for (i = 0; i < MAX_LEDS; i++)
   {
      ledoverlay_cur->setup[i] = 0;
      ledoverlay_cur->map[i]   = settings->uints.led_map[i];

      if (ledoverlay_cur->map[i] >= 0)
         input_overlay_set_visibility(ledoverlay_cur->map[i],
               OVERLAY_VISIBILITY_HIDDEN);
   }
}

static void overlay_free(void)
{
}

static void overlay_set(int led, int state)
{
   int gpio = 0;
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   gpio = ledoverlay_cur->map[led];

   if (gpio < 0)
      return;

   input_overlay_set_visibility(gpio,
         state ? OVERLAY_VISIBILITY_VISIBLE
         : OVERLAY_VISIBILITY_HIDDEN);
}

const led_driver_t overlay_led_driver = {
   overlay_init,
   overlay_free,
   overlay_set,
   "Overlay"
};
