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
#include <android/keycodes.h>
#include "android-general.h"
#include "../../../general.h"
#include "../../../driver.h"
#include "input_android.h"

enum {
    AKEYCODE_BUTTON_1        = 188,
    AKEYCODE_BUTTON_2        = 189,
    AKEYCODE_BUTTON_3        = 190,
    AKEYCODE_BUTTON_4        = 191,
    AKEYCODE_BUTTON_5        = 192,
    AKEYCODE_BUTTON_6        = 193,
    AKEYCODE_BUTTON_7        = 194,
    AKEYCODE_BUTTON_8        = 195,
    AKEYCODE_BUTTON_9        = 196,
    AKEYCODE_BUTTON_10       = 197,
    AKEYCODE_BUTTON_11       = 198,
    AKEYCODE_BUTTON_12       = 199,
    AKEYCODE_BUTTON_13       = 200,
    AKEYCODE_BUTTON_14       = 201,
    AKEYCODE_BUTTON_15       = 202,
    AKEYCODE_BUTTON_16       = 203,
};

enum {
    AKEYSTATE_DONT_PROCESS   = 0,
    AKEYSTATE_PROCESS        = 1,
};

static unsigned pads_connected;
static android_input_state_t state[MAX_PADS];

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
   int id, i;
   bool found_existing_id = false;

   id = AInputEvent_getDeviceId(event);

   for (i = 0; i < pads_connected; i++)
   {
      if (id == state[i].id)
      {
         found_existing_id = true;
         break;
      }
   }

   if(!found_existing_id)
   {
      state[pads_connected++].id = id;
      i = pads_connected;
   }

   {
      bool do_poll = false;
      float x, y;
      int action, keycode, source, type;
      action = AKEY_EVENT_NO_ACTION;

      type    = AInputEvent_getType(event);
      source  = AInputEvent_getSource(event);
      keycode = AKeyEvent_getKeyCode(event);

      switch(source)
      {
         case AINPUT_SOURCE_DPAD:
            RARCH_LOG("AINPUT_SOURCE_DPAD, pad: %d, keycode: %d.\n", i, keycode);
            break;
         case AINPUT_SOURCE_TOUCHSCREEN:
            RARCH_LOG("AINPUT_SOURCE_TOUCHSCREEN, pad: %d, keycode: %d.\n", i, keycode);
            break; 
         case AINPUT_SOURCE_TOUCHPAD:
            RARCH_LOG("AINPUT_SOURCE_TOUCHPAD, pad: %d, keycode: %d.\n", i, keycode);
            break;
         case AINPUT_SOURCE_ANY:
            RARCH_LOG("AINPUT_SOURCE_ANY, pad: %d, keycode: %d.\n", i, keycode);
            break;
         case 0:
            default:
            RARCH_LOG("AINPUT_SOURCE_DEFAULT, pad: %d, keycode: %d.\n", i, keycode);
            break;
      }

      switch(type)
      {
         case AINPUT_EVENT_TYPE_KEY:
            RARCH_LOG("AINPUT_EVENT_TYPE_KEY, pad: %d.\n", i);
	    action  = AKeyEvent_getAction(event);
            do_poll = true;
            break;
         case AINPUT_EVENT_TYPE_MOTION:
            x = AMotionEvent_getX(event, 0);
            y = AMotionEvent_getY(event, 0);
            RARCH_LOG("AINPUT_EVENT_TYPE_MOTION, pad: %d, x: %f, y: %f.\n", i, x, y);
            break;
      }

      if(action != AKEY_EVENT_NO_ACTION)
      {
         switch(action)
         {
            case AKEY_EVENT_ACTION_DOWN:
               RARCH_LOG("AKEY_EVENT_ACTION_DOWN, pad: %d, keycode: %d.\n", i, keycode);
               break;
            case AKEY_EVENT_ACTION_UP:
               RARCH_LOG("AKEY_EVENT_ACTION_UP, pad: %d, keycode: %d.\n", i, keycode);
	       do_poll = true;
               break;
            case AKEY_EVENT_ACTION_MULTIPLE:
               RARCH_LOG("AKEY_EVENT_ACTION_MULTIPLE, pad: %d, keycode: %d.\n", i, keycode);
               break;
            default:
               RARCH_LOG("AKEY_EVENT_NO_ACTION, pad: %d, keycode: %d.\n", i, keycode);
               break;
         }
      }

      state[i].state = 0;

      if(do_poll)
      {
         state[i].state |= (keycode == AKEYCODE_BUTTON_10) ? ANDROID_GAMEPAD_START : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_12) ? ANDROID_GAMEPAD_R3 : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_11) ? ANDROID_GAMEPAD_L3 : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_9 ) ? ANDROID_GAMEPAD_SELECT : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_4 ) ? ANDROID_GAMEPAD_TRIANGLE : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_1 ) ? ANDROID_GAMEPAD_SQUARE : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_2 ) ? ANDROID_GAMEPAD_CROSS : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_3 ) ? ANDROID_GAMEPAD_CIRCLE : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_6 ) ? ANDROID_GAMEPAD_R1 : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_5 ) ? ANDROID_GAMEPAD_L1 : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_8 ) ? ANDROID_GAMEPAD_R2 : 0;
	 state[i].state |= (keycode == AKEYCODE_BUTTON_7 ) ? ANDROID_GAMEPAD_L2 : 0;
      }

   }

   return 1;
}

static void *android_input_init(void)
{
   void *libandroid = 0;

   g_android.app->onInputEvent = engine_handle_input;
   pads_connected = 0;

   return (void*)-1;
}

static void android_input_poll(void *data)
{
   (void)data;
}

static int16_t android_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   unsigned player = port; 
   uint64_t button = binds[player][id].joykey;
   int16_t retval = 0;

   if((player < pads_connected))
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            retval = (state[player].state & button) ? 1 : 0;
            if(retval != 0)
            {
               RARCH_LOG("state: %d, player: %d.\n", retval, player);
            }
            break;
      }
    }


   return retval;
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

