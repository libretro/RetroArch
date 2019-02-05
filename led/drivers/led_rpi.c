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
#include "../../verbosity.h"

typedef struct
{
   int setup[MAX_LEDS];
   int map[MAX_LEDS];
} rpiled_t;

static rpiled_t curins;
static rpiled_t *cur = &curins;

static void rpi_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   for(i = 0; i < MAX_LEDS; i++)
   {
      cur->setup[i] = 0;
      cur->map[i]   = settings->uints.led_map[i];
      RARCH_LOG("[LED]: rpi map[%d]=%d\n", i, cur->map[i]);
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

   if(!fp)
   {
      RARCH_WARN("[LED]: failed to set GPIO %d\n", gpio);
      return -1;
   }

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

      if(!fp)
      {
         RARCH_WARN("[LED]: failed to export GPIO %d\n", gpio);
         return -1;
      }

      fprintf(fp,"%d\n", gpio);
      fclose(fp);

      snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", gpio);
      fp = fopen(buf, "w");
   }

   if(!fp)
   {
      RARCH_WARN("[LED]: failed to set direction GPIO %d\n",
            gpio);
      return -1;
   }

   fprintf(fp, "out\n");
   fclose(fp);
   return 1;
}

static void rpi_set(int led, int state)
{
   int gpio = 0;

   if((led < 0) || (led >= MAX_LEDS))
   {
      RARCH_WARN("[LED]: invalid led %d\n", led);
      return;
   }

   gpio = cur->map[led];
   if(gpio <= 0)
      return;

   if(cur->setup[led] == 0)
   {
      RARCH_LOG("[LED]: rpi setup led %d gpio %d\n",
            led, gpio, state);
      cur->setup[led] = setup_gpio(gpio);
      if(cur->setup[led] <= 0)
      {
         RARCH_WARN("[LED]: failed to setup led %d gpio %d\n",
               led, gpio);
      }
   }
   if(cur->setup[led] > 0)
   {
      RARCH_LOG("[LED]: rpi LED driver set led %d gpio %d = %d\n",
            led, gpio, state);
      set_gpio(gpio, state);
   }
}

const led_driver_t rpi_led_driver = {
   rpi_init,
   rpi_free,
   rpi_set,
   "rpi"
};
