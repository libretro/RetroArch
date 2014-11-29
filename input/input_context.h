/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef INPUT_CONTEXT_H__
#define INPUT_CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <boolean.h>
#include "../libretro.h"

enum retro_rumble_effect;

struct rarch_joypad_driver
{
   bool (*init)(void);
   bool (*query_pad)(unsigned);
   void (*destroy)(void);
   bool (*button)(unsigned, uint16_t);
   int16_t (*axis)(unsigned, uint32_t);
   void (*poll)(void);
   bool (*set_rumble)(unsigned, enum retro_rumble_effect, uint16_t);
   const char *(*name)(unsigned);

   const char *ident;
};
    
typedef struct rarch_joypad_driver rarch_joypad_driver_t;

const char* config_get_joypad_driver_options(void);

/* If ident points to NULL or a zero-length string,
 * equivalent to calling input_joypad_init_first(). */
const rarch_joypad_driver_t *input_joypad_init_driver(const char *ident);

const rarch_joypad_driver_t *input_joypad_init_first(void);

extern rarch_joypad_driver_t dinput_joypad;
extern rarch_joypad_driver_t linuxraw_joypad;
extern rarch_joypad_driver_t parport_joypad;
extern rarch_joypad_driver_t udev_joypad;
extern rarch_joypad_driver_t winxinput_joypad;
extern rarch_joypad_driver_t sdl_joypad;
extern rarch_joypad_driver_t ps3_joypad;
extern rarch_joypad_driver_t psp_joypad;
extern rarch_joypad_driver_t xdk_joypad;
extern rarch_joypad_driver_t gx_joypad;
extern rarch_joypad_driver_t apple_hid_joypad;
extern rarch_joypad_driver_t apple_ios_joypad;
extern rarch_joypad_driver_t android_joypad;
extern rarch_joypad_driver_t qnx_joypad;
extern rarch_joypad_driver_t null_joypad;

#ifdef __cplusplus
}
#endif

#endif
