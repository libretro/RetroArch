/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "android-general.h"
#include "../../../general.h"
#include "../../../driver.h"

/**
 * Process the next input event.
 */
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
   if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
   {
      g_android.animating = 1;
      g_android.state.x = AMotionEvent_getX(event, 0);
      g_android.state.y = AMotionEvent_getY(event, 0);
      //RARCH_LOG("AINPUT_EVENT_TYPE_MOTION - x: %d, y: %d.\n");
      return 1;
   }
   return 0;
}

static void *android_input_init(void)
{
   g_android.app->onInputEvent = engine_handle_input;
   return (void*)-1;
}

static void android_input_poll(void *data)
{
   (void)data;
}

static int16_t android_input_state(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)data;
   (void)retro_keybinds;
   (void)port;
   (void)device;
   (void)index;
   (void)id;

   return 0;
}

static bool android_input_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;

   return false;
}

static void android_input_free(void *data)
{
   (void)data;
}

const input_driver_t input_android = {
   android_input_init,
   android_input_poll,
   android_input_state,
   android_input_key_pressed,
   android_input_free,
   "android_input",
};

