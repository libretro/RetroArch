#include <stdio.h>
#include "../led_driver.h"
#include "../led_defines.h"

#include "../../input/input_overlay.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"


typedef struct
{
   int setup[MAX_LEDS];
   int map[MAX_LEDS];
} overlayled_t;

static overlayled_t curins;
static overlayled_t *cur = &curins;

static void overlay_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   RARCH_LOG("[LED]: overlay LED driver init\n");

   for (i = 0; i < MAX_LEDS; i++)
   {
      cur->setup[i] = 0;
      cur->map[i]   = settings->uints.led_map[i];
      RARCH_LOG("[LED]: overlay map[%d]=%d\n",i,cur->map[i]);

      if (cur->map[i] >= 0)
         input_overlay_set_visibility(cur->map[i],
               OVERLAY_VISIBILITY_HIDDEN);
   }
}

static void overlay_free(void)
{
    RARCH_LOG("[LED]: overlay LED driver free\n");
}

static void overlay_set(int led, int state)
{
   int gpio = 0;
   if ((led < 0) || (led >= MAX_LEDS))
   {
      RARCH_WARN("[LED]: invalid led %d\n", led);
      return;
   }

   gpio = cur->map[led];

   if (gpio < 0)
      return;

   input_overlay_set_visibility(gpio,
         state ? OVERLAY_VISIBILITY_VISIBLE
         : OVERLAY_VISIBILITY_HIDDEN);

   RARCH_LOG("[LED]: set visibility %d %d\n", gpio, state);
}

const led_driver_t overlay_led_driver = {
   overlay_init,
   overlay_free,
   overlay_set,
   "Overlay"
};
