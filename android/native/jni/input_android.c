/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include <dlfcn.h>
#include "android-general.h"
#include "../../../general.h"
#include "../../../driver.h"

/* Process the next input event */
static float AMotionEvent_getAxisValue(const AInputEvent* motion_event, int32_t axis, size_t pointer_index);

#define AKEY_EVENT_NO_ACTION 255

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
   float x,y;
   int action, keycode, source, type, id;

   action = AKEY_EVENT_NO_ACTION;
   type   = AInputEvent_getType(event);
   source = AInputEvent_getSource(event);
   id     = AInputEvent_getDeviceId(event);

   switch(type)
   {
      case AINPUT_EVENT_TYPE_KEY:
         action = AKeyEvent_getAction(event);
         keycode = AKeyEvent_getKeyCode(event);
         RARCH_LOG("AINPUT_EVENT_TYPE_KEY, id: %d.\n", id);
         break;
      case AINPUT_EVENT_TYPE_MOTION:
         x = AMotionEvent_getX(event, 0);
         y = AMotionEvent_getY(event, 0);
         RARCH_LOG("AINPUT_EVENT_TYPE_MOTION, id: %d, x: %f, y: %f.\n", id, x, y);
         break;
   }

#if 0
   x = AMotionEvent_getAxisValue(event, 0, 0);
   y = AMotionEvent_getAxisValue(event, 0, 0);
#endif
   if(action != AKEY_EVENT_NO_ACTION)
   {
      switch(action)
      {
         case AKEY_EVENT_ACTION_DOWN:
            RARCH_LOG("AKEY_EVENT_ACTION_DOWN, id: %d, keycode: %d.\n", id, keycode);
            break;
         case AKEY_EVENT_ACTION_UP:
            RARCH_LOG("AKEY_EVENT_ACTION_UP, id: %d, keycode: %d.\n", id, keycode);
            break;
         case AKEY_EVENT_ACTION_MULTIPLE:
            RARCH_LOG("AKEY_EVENT_ACTION_MULTIPLE, id: %d, keycode: %d.\n", id, keycode);
            break;
         default:
            RARCH_LOG("AKEY_EVENT_NO_ACTION, id: %d, keycode: %d.\n", id, keycode);
            break;
      }
   }

   switch(source)
   {
      case AINPUT_SOURCE_DPAD:
         RARCH_LOG("AINPUT_SOURCE_DPAD, id: %d, keycode: %d.\n", id, keycode);
         break;
      case AINPUT_SOURCE_TOUCHSCREEN:
         RARCH_LOG("AINPUT_SOURCE_TOUCHSCREEN, id: %d, keycode: %d.\n", id, keycode);
         break; 
      case AINPUT_SOURCE_TOUCHPAD:
         RARCH_LOG("AINPUT_SOURCE_TOUCHPAD, id: %d, keycode: %d.\n", id, keycode);
         break;
      case AINPUT_SOURCE_ANY:
         RARCH_LOG("AINPUT_SOURCE_ANY, id: %d, keycode: %d.\n", id, keycode);
         break;
      default:
         RARCH_LOG("AINPUT_SOURCE_DEFAULT, id: %d, keycode: %d.\n", id, keycode);
         break;
   }

   return 1;
}

static void *android_input_init(void)
{
   void *libandroid = 0;

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

   switch (key)
   {
      case RARCH_QUIT_KEY:
	if(g_android.init_quit)
           return true;
        else
           return false;
        break;
      default:
         (void)0;
   }

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

