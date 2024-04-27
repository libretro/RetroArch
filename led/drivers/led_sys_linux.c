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
#include <compat/strl.h>

#include "../led_driver.h"
#include "../led_defines.h"

#include "../../configuration.h"

typedef struct
{
   int setup[MAX_LEDS];
   int map[MAX_LEDS];
} sysled_t;

/* TODO/FIXME - static globals */
static sysled_t sys_curins;
static sysled_t *sys_cur = &sys_curins;

static void sys_led_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   for (i = 0; i < MAX_LEDS; i++)
   {
      sys_cur->setup[i] = 0;
      sys_cur->map[i]   = settings->uints.led_map[i];
   }
}

static void sys_led_free(void)
{
   int i;

   for (i = 0; i < MAX_LEDS; i++)
   {
      sys_cur->setup[i] = 0;
      sys_cur->map[i]   = 0;
   }
}

static int set_sysled(int sysled, int value)
{
   FILE *fp;
   char buf[256];
   snprintf(buf, sizeof(buf), "/sys/class/leds/led%d/brightness", sysled);

   /* Failed to set LED? */
   if (!(fp = fopen(buf, "w")))
      return -1;

   /* Simplified: max_brightness could be taken into account */
   /* Pi Zero may have reversed brightness? */
   fprintf(fp, "%d\n", value ? 1 : 0);
   fclose(fp);
   return 1;
}

static int setup_sysled(int sysled)
{
   FILE *fp;
   char buf[256];
   snprintf(buf, sizeof(buf), "/sys/class/leds/led%d/trigger", sysled);
   
   if (!(fp = fopen(buf, "w")))
      return -1;

   /* TODO: read actual trigger in [] and restore on exit */
   fprintf(fp, "none");
   fclose(fp);
   return 1;
}

static void sys_led_set(int led, int state)
{
   int sysled = 0;

   /* Invalid LED? */
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   sysled = sys_cur->map[led];
   if (sysled < 0)
      return;

   if (sys_cur->setup[led] == 0)
      sys_cur->setup[led] = setup_sysled(sysled);
   if (sys_cur->setup[led] > 0)
      set_sysled(sysled, state);
}

const led_driver_t sys_led_driver = {
   sys_led_init,
   sys_led_free,
   sys_led_set,
   "sysled"
};
