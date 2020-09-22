/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "../led_driver.h"
#include "../led_defines.h"

#include "../../configuration.h"

typedef struct
{
   int setup[MAX_LEDS];
   int map[MAX_LEDS];
} rpiled_t;

/* TODO/FIXME - static globals */
static rpiled_t rpi_curins;
static rpiled_t *rpi_cur = &rpi_curins;

static void rpi_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   for (i = 0; i < MAX_LEDS; i++)
   {
      rpi_cur->setup[i] = 0;
      rpi_cur->map[i]   = settings->uints.led_map[i];
   }
}

static void rpi_free(void)
{
}

static int set_gpio(int gpio, int value)
{
   FILE *fp;
   char buf[256];
   snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpio);
   fp = fopen(buf, "w");

   /* Failed to set GPIO? */
   if (!fp)
      return -1;

   fprintf(fp, "%d\n", value ? 1 : 0);
   fclose(fp);
   return 1;
}

static int setup_gpio(int gpio)
{
   FILE *fp;
   char buf[256];
   snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", gpio);
   fp = fopen(buf, "w");

   if(!fp)
   {
      snprintf(buf, sizeof(buf), "/sys/class/gpio/export");
      fp = fopen(buf, "w");

      /* Failed to export GPIO? */
      if (!fp)
         return -1;

      fprintf(fp,"%d\n", gpio);
      fclose(fp);

      snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", gpio);
      fp = fopen(buf, "w");
   }

   /* Failed to set direction GPIO? */
   if (!fp)
      return -1;

   fprintf(fp, "out\n");
   fclose(fp);
   return 1;
}

static void rpi_set(int led, int state)
{
   int gpio = 0;

   /* Invalid LED? */
   if((led < 0) || (led >= MAX_LEDS))
      return;

   gpio = rpi_cur->map[led];
   if(gpio <= 0)
      return;

   if(rpi_cur->setup[led] == 0)
      rpi_cur->setup[led] = setup_gpio(gpio);
   if(rpi_cur->setup[led] > 0)
      set_gpio(gpio, state);
}

const led_driver_t rpi_led_driver = {
   rpi_init,
   rpi_free,
   rpi_set,
   "rpi"
};
