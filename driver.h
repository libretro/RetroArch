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

#include "configuration.h"
#include "retroarch_types.h"

RETRO_BEGIN_DECLS

enum
{
   DRIVER_AUDIO = 0,
   DRIVER_VIDEO,
   DRIVER_INPUT,
   DRIVER_CAMERA,
   DRIVER_LOCATION,
   DRIVER_MENU,
   DRIVERS_VIDEO_INPUT,
   DRIVER_BLUETOOTH,
   DRIVER_WIFI,
   DRIVER_LED,
   DRIVER_MIDI
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
   DRIVER_BLUETOOTH_MASK    = 1 << DRIVER_BLUETOOTH,
   DRIVER_WIFI_MASK         = 1 << DRIVER_WIFI,
   DRIVER_LED_MASK          = 1 << DRIVER_LED,
   DRIVER_MIDI_MASK         = 1 << DRIVER_MIDI
};

enum driver_ctl_state
{
   RARCH_DRIVER_CTL_NONE = 0,

   /* Sets monitor refresh rate to new value by calling
    * video_monitor_set_refresh_rate(). Subsequently
    * calls audio_monitor_set_refresh_rate(). */
   RARCH_DRIVER_CTL_SET_REFRESH_RATE,

   RARCH_DRIVER_CTL_FIND_FIRST,

   RARCH_DRIVER_CTL_FIND_LAST,

   RARCH_DRIVER_CTL_FIND_PREV,

   RARCH_DRIVER_CTL_FIND_NEXT
};

typedef struct driver_ctx_info
{
   const char *label;
   char *s;
   ssize_t len;
} driver_ctx_info_t;

bool driver_ctl(enum driver_ctl_state state, void *data);

/**
 * driver_find_index:
 * @label              : string of driver type to be found.
 * @drv                : identifier of driver to be found.
 *
 * Find index of the driver, based on @label.
 *
 * Returns: -1 if no driver based on @label and @drv found, otherwise
 * index number of the driver found in the array.
 **/
int driver_find_index(const char *label, const char *drv);

/* Sets audio and video drivers to nonblock state.
 *
 * If nonblock state is false, sets blocking state for both
 * audio and video drivers instead. */
void driver_set_nonblock_state(void);

/**
 * drivers_init:
 * @flags              : Bitmask of drivers to initialize.
 *
 * Initializes drivers.
 * @flags determines which drivers get initialized.
 **/
void drivers_init(settings_t *settings, int flags,
      bool verbosity_enabled);

/**
 * Driver ownership - set this to true if the platform in
 * question needs to 'own'
 * the respective handle and therefore skip regular RetroArch
 * driver teardown/reiniting procedure.
 *
 * If  to true, the 'free' function will get skipped. It is
 * then up to the driver implementation to properly handle
 * 'reiniting' inside the 'init' function and make sure it
 * returns the existing handle instead of allocating and
 * returning a pointer to a new handle.
 *
 * Typically, if a driver intends to make use of this, it should
 * set this to true at the end of its 'init' function.
 **/
void driver_uninit(int flags);

void retro_input_poll_null(void);

void retroarch_deinit_drivers(struct retro_callbacks *cbs);

RETRO_END_DECLS

#endif
