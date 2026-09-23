#include <stdint.h>
#include "../led_driver.h"
#include "../led_defines.h"

#include "../../input/input_overlay.h"

/* The overlay shows the LEDs: a pack's _led images, or for a pack that
 * names none, the slots ledN_map points at. Which images those are is
 * the overlay's to work out (input_overlay_image_hidden()); this only
 * reports the core's LEDs. */

static void overlay_init(void)
{
   input_overlay_leds_enable(true);
}

static void overlay_free(void)
{
   input_overlay_leds_enable(false);
}

static void overlay_set(int led, int state)
{
   input_overlay_set_led(led, state != 0);
}

const led_driver_t overlay_led_driver = {
   overlay_init,
   overlay_free,
   overlay_set,
   "Overlay"
};
