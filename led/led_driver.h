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

#ifndef __LED_DRIVER__H
#define __LED_DRIVER__H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_common_api.h>
#include <retro_environment.h>

RETRO_BEGIN_DECLS

typedef struct led_driver
{
   void (*init)(void);
   void (*free)(void);
   void (*set_led)(int led, int value);
   const char *ident;
} led_driver_t;

bool led_driver_init(void);

void led_driver_free(void);

void led_driver_set_led(int led, int value);

extern const led_driver_t overlay_led_driver;
extern const led_driver_t rpi_led_driver;

RETRO_END_DECLS

#endif
