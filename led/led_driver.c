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
#include <string/stdstring.h>

#include "led_driver.h"
#include "../configuration.h"
#include "../verbosity.h"

static const led_driver_t *current_led_driver = NULL;

static void null_led_init(void) { }
static void null_led_free(void) { }
static void null_led_set(int led, int state) { }

static const led_driver_t null_led_driver = {
   null_led_init,
   null_led_free,
   null_led_set,
   "null"
};

bool led_driver_init(void)
{
   settings_t *settings = config_get_ptr();
   char *drivername     = settings ? settings->arrays.led_driver : NULL;

   if(!drivername)
      drivername = (char*)"null";

   current_led_driver = &null_led_driver;

#ifdef HAVE_OVERLAY
   if(string_is_equal("overlay", drivername))
      current_led_driver = &overlay_led_driver;
#endif

#if HAVE_RPILED
   if(string_is_equal("rpi", drivername))
      current_led_driver = &rpi_led_driver;
#endif

   RARCH_LOG("[LED]: LED driver = '%s' %p\n",
         drivername, current_led_driver);

   if(current_led_driver)
      (*current_led_driver->init)();

   return true;
}

void led_driver_free(void)
{
   if(current_led_driver)
      (*current_led_driver->free)();
}

void led_driver_set_led(int led, int value)
{
   if(current_led_driver)
      (*current_led_driver->set_led)(led, value);
}
