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
#include <fcntl.h>
#include <unistd.h>
#include <compat/strl.h>

#include "../led_driver.h"
#include "../led_defines.h"

#include "../../configuration.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifndef RPILED_GPIO_ROOT
#define RPILED_GPIO_ROOT "/sys/class/gpio"
#endif

/* An exported GPIO's value file is held open from the first set, so a
 * state change is one pwrite of a constant. */
typedef struct
{
   int fd[MAX_LEDS];     /* value file; -1 not set up, -2 failed */
   int state[MAX_LEDS];  /* 1 on, 0 off, -1 nothing written yet */
   int map[MAX_LEDS];
} rpiled_t;

/* TODO/FIXME - static globals */
static rpiled_t rpi_curins;
static rpiled_t *rpi_cur = &rpi_curins;

static void rpi_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   for (i = 0; i < MAX_LEDS; i++)
   {
      rpi_cur->fd[i]    = -1;
      rpi_cur->state[i] = -1;
      rpi_cur->map[i]   = settings ? (int)settings->uints.led_map[i] : 0;
   }
}

static void rpi_free(void)
{
   int i;

   for (i = 0; i < MAX_LEDS; i++)
   {
      if (rpi_cur->fd[i] >= 0)
         close(rpi_cur->fd[i]);
      rpi_cur->fd[i]    = -1;
      rpi_cur->state[i] = -1;
      rpi_cur->map[i]   = 0;
   }
}

/* Exports @gpio as an output and opens its value file; -2 on failure. */
static int setup_gpio(int gpio)
{
   int fd;
   FILE *fp;
   char buf[256];
   snprintf(buf, sizeof(buf), RPILED_GPIO_ROOT "/gpio%d/direction", gpio);
   
   if (!(fp = fopen(buf, "w")))
   {
      strlcpy_lit(buf, RPILED_GPIO_ROOT "/export", sizeof(buf));

      /* Failed to export GPIO? */
      if (!(fp = fopen(buf, "w")))
         return -2;

      fprintf(fp,"%d\n", gpio);
      fclose(fp);

      snprintf(buf, sizeof(buf), RPILED_GPIO_ROOT "/gpio%d/direction", gpio);
      fp = fopen(buf, "w");
   }

   /* Failed to set direction GPIO? */
   if (!fp)
      return -2;

   fprintf(fp, "out\n");
   fclose(fp);

   snprintf(buf, sizeof(buf), RPILED_GPIO_ROOT "/gpio%d/value", gpio);
   if ((fd = open(buf, O_WRONLY | O_CLOEXEC)) < 0)
      return -2;
   return fd;
}

static void rpi_set(int led, int state)
{
   static const char value[2][2] = { { '0', '\n' }, { '1', '\n' } };
   int gpio = 0;
   int on   = (state != 0);

   /* Invalid LED? */
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   gpio = rpi_cur->map[led];
   if (gpio <= 0)
      return;

   if (rpi_cur->fd[led] == -1)
      rpi_cur->fd[led] = setup_gpio(gpio);
   if (     rpi_cur->fd[led] >= 0
         && rpi_cur->state[led] != on
         && pwrite(rpi_cur->fd[led], value[on], 2, 0) == 2)
      rpi_cur->state[led] = on;
}

const led_driver_t rpi_led_driver = {
   rpi_init,
   rpi_free,
   rpi_set,
   "rpi"
};
