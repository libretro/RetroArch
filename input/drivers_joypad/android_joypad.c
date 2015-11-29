/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2013-2014 - Steven Crowe
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms *  of the GNU General Public License as published by the Free Software Found-
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

#include "../drivers_keyboard/keyboard_event_android.h"

static int16_t analog_state[MAX_PADS][MAX_AXIS];
static int8_t hat_state[MAX_PADS][2];

static void engine_handle_dpad_default(AInputEvent *event, int port, int source)
{
   size_t motion_ptr = AMotionEvent_getAction(event) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x           = AMotionEvent_getX(event, motion_ptr);
   float y           = AMotionEvent_getY(event, motion_ptr);

   analog_state[port][0] = (int16_t)(x * 32767.0f);
   analog_state[port][1] = (int16_t)(y * 32767.0f);
}

static void engine_handle_dpad_getaxisvalue(AInputEvent *event, int port, int source)
{
   size_t motion_ptr = AMotionEvent_getAction(event) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
   float x           = AMotionEvent_getAxisValue(event, AXIS_X, motion_ptr);
   float y           = AMotionEvent_getAxisValue(event, AXIS_Y, motion_ptr);
   float z           = AMotionEvent_getAxisValue(event, AXIS_Z, motion_ptr);
   float rz          = AMotionEvent_getAxisValue(event, AXIS_RZ, motion_ptr);
   float hatx        = AMotionEvent_getAxisValue(event, AXIS_HAT_X, motion_ptr);
   float haty        = AMotionEvent_getAxisValue(event, AXIS_HAT_Y, motion_ptr);
   float ltrig       = AMotionEvent_getAxisValue(event, AXIS_LTRIGGER, motion_ptr);
   float rtrig       = AMotionEvent_getAxisValue(event, AXIS_RTRIGGER, motion_ptr);
   float brake       = AMotionEvent_getAxisValue(event, AXIS_BRAKE, motion_ptr);
   float gas         = AMotionEvent_getAxisValue(event, AXIS_GAS, motion_ptr);

   hat_state[port][0] = (int)hatx;
   hat_state[port][1] = (int)haty;

   /* XXX: this could be a loop instead, but do we really want to
    * loop through every axis? */
   analog_state[port][0] = (int16_t)(x * 32767.0f);
   analog_state[port][1] = (int16_t)(y * 32767.0f);
   analog_state[port][2] = (int16_t)(z * 32767.0f);
   analog_state[port][3] = (int16_t)(rz * 32767.0f);
#if 0
   analog_state[port][4] = (int16_t)(hatx * 32767.0f);
   analog_state[port][5] = (int16_t)(haty * 32767.0f);
#endif
   analog_state[port][6] = (int16_t)(ltrig * 32767.0f);
   analog_state[port][7] = (int16_t)(rtrig * 32767.0f);
   analog_state[port][8] = (int16_t)(brake * 32767.0f);
   analog_state[port][9] = (int16_t)(gas * 32767.0f);
}

static const char *android_joypad_name(unsigned pad)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->input.device_names[pad] : NULL;
}

static bool android_joypad_init(void *data)
{
   engine_handle_dpad         = engine_handle_dpad_default;

   if ((dlopen("/system/lib/libandroid.so", RTLD_LOCAL | RTLD_LAZY)) == 0)
   {
      RARCH_WARN("Unable to open libandroid.so\n");
      return true;
   }

   if ((p_AMotionEvent_getAxisValue = dlsym(RTLD_DEFAULT,
               "AMotionEvent_getAxisValue")))
   {
      RARCH_LOG("Set engine_handle_dpad to 'Get Axis Value' (for reading extra analog sticks)");
      engine_handle_dpad = engine_handle_dpad_getaxisvalue;
   }

   return true;
}

static bool android_joypad_button(unsigned port, uint16_t joykey)
{
   uint8_t *buf             = android_keyboard_state_get(port);

   if (port >= MAX_PADS)
      return false;

   if (GET_HAT_DIR(joykey))
   {
      unsigned h = GET_HAT(joykey);
      if (h > 0)
         return false;

      switch (GET_HAT_DIR(joykey))
      {
         case HAT_LEFT_MASK:
            return hat_state[port][0] == -1;
         case HAT_RIGHT_MASK:
            return hat_state[port][0] ==  1;
         case HAT_UP_MASK:
            return hat_state[port][1] == -1;
         case HAT_DOWN_MASK:
            return hat_state[port][1] ==  1;
         default:
            return false;
      }
   }

   return joykey < LAST_KEYCODE && BIT_GET(buf, joykey);
}

static int16_t android_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int val                  = 0;

   if (joyaxis == AXIS_NONE)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < MAX_AXIS)
   {
      val = analog_state[port][AXIS_NEG_GET(joyaxis)];
      if (val > 0)
         val = 0;
   }
   else if (AXIS_POS_GET(joyaxis) < MAX_AXIS)
   {
      val = analog_state[port][AXIS_POS_GET(joyaxis)];
      if (val < 0)
         val = 0;
   }

   return val;
}

static void android_joypad_poll(void)
{
}

static bool android_joypad_query_pad(unsigned pad)
{
   driver_t *driver         = driver_get_ptr();
   android_input_t *android = driver ? (android_input_t*)driver->input_data : NULL;
   return (pad < MAX_USERS && pad < android->pads_connected);
}


static void android_joypad_destroy(void)
{
   unsigned i, j;

   for (i = 0; i < MAX_PADS; i++)
   {
      for (j = 0; j < 2; j++)
         hat_state[i][j]    = 0;
      for (j = 0; j < MAX_AXIS; j++)
         analog_state[i][j] = 0;
   }
}

input_device_driver_t android_joypad = {
   android_joypad_init,
   android_joypad_query_pad,
   android_joypad_destroy,
   android_joypad_button,
   NULL,
   android_joypad_axis,
   android_joypad_poll,
   NULL,
   android_joypad_name,
   "android",
};
