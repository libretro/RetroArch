/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __RARCH_DRIVER__H
#define __RARCH_DRIVER__H

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define DRIVERS_CMD_ALL \
      ( DRIVER_AUDIO_MASK \
      | DRIVER_VIDEO_MASK \
      | DRIVER_INPUT_MASK \
      | DRIVER_CAMERA_MASK \
      | DRIVER_LOCATION_MASK \
      | DRIVER_MENU_MASK \
      | DRIVERS_VIDEO_INPUT_MASK \
      | DRIVER_WIFI_MASK \
      | DRIVER_LED_MASK )

#define DRIVERS_CMD_ALL_BUT_MENU \
      ( DRIVER_AUDIO_MASK \
      | DRIVER_VIDEO_MASK \
      | DRIVER_INPUT_MASK \
      | DRIVER_CAMERA_MASK \
      | DRIVER_LOCATION_MASK \
      | DRIVERS_VIDEO_INPUT_MASK \
      | DRIVER_WIFI_MASK \
      | DRIVER_LED_MASK )

enum
{
   DRIVER_AUDIO = 0,
   DRIVER_VIDEO,
   DRIVER_INPUT,
   DRIVER_CAMERA,
   DRIVER_LOCATION,
   DRIVER_MENU,
   DRIVERS_VIDEO_INPUT,
   DRIVER_WIFI,
   DRIVER_LED
};

enum
{
   DRIVER_AUDIO_MASK        = 1 << DRIVER_AUDIO,
   DRIVER_VIDEO_MASK        = 1 << DRIVER_VIDEO,
   DRIVER_INPUT_MASK        = 1 << DRIVER_INPUT,
   DRIVER_CAMERA_MASK       = 1 << DRIVER_CAMERA,
   DRIVER_LOCATION_MASK     = 1 << DRIVER_LOCATION,
   DRIVER_MENU_MASK         = 1 << DRIVER_MENU,
   DRIVERS_VIDEO_INPUT_MASK = 1 << DRIVERS_VIDEO_INPUT,
   DRIVER_WIFI_MASK         = 1 << DRIVER_WIFI,
   DRIVER_LED_MASK          = 1 << DRIVER_LED
};

enum driver_ctl_state
{
   RARCH_DRIVER_CTL_NONE = 0,
   RARCH_DRIVER_CTL_DEINIT,

   /* Attempts to find a default driver for
    * all driver types.
    *
    * Should be run before RARCH_DRIVER_CTL_INIT.
    */
   RARCH_DRIVER_CTL_INIT_PRE,

   /* Sets monitor refresh rate to new value by calling
    * video_monitor_set_refresh_rate(). Subsequently
    * calls audio_monitor_set_refresh_rate(). */
   RARCH_DRIVER_CTL_SET_REFRESH_RATE,

   /* Update the system Audio/Video information.
    * Will reinitialize audio/video drivers.
    * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO. */
   RARCH_DRIVER_CTL_UPDATE_SYSTEM_AV_INFO,

   RARCH_DRIVER_CTL_FIND_FIRST,

   RARCH_DRIVER_CTL_FIND_LAST,

   RARCH_DRIVER_CTL_FIND_PREV,

   RARCH_DRIVER_CTL_FIND_NEXT,

   /* Find index of the driver, based on @label. */
   RARCH_DRIVER_CTL_FIND_INDEX

};

typedef struct driver_ctx_info
{
   const char *label;
   char *s;
   ssize_t len;
} driver_ctx_info_t;

bool driver_ctl(enum driver_ctl_state state, void *data);

/* Sets audio and video drivers to nonblock state.
 *
 * If nonblock state is false, sets blocking state for both
 * audio and video drivers instead. */
void driver_set_nonblock_state(void);

void driver_uninit(int flags);

void drivers_init(int flags);

RETRO_END_DECLS

#endif
