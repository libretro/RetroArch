/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
 *  Copyright (C) 2013-2014 - Steven Crowe
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

static const char *android_joypad_name(unsigned pad)
{
   return g_settings.input.device_names[pad];
}

static bool android_joypad_init(void)
{
   unsigned autoconf_pad;

   for (autoconf_pad = 0; autoconf_pad < MAX_PLAYERS; autoconf_pad++)
   {
      strlcpy(g_settings.input.device_names[autoconf_pad],
            android_joypad_name(autoconf_pad),
            sizeof(g_settings.input.device_names[autoconf_pad]));
      input_config_autoconfigure_joypad(autoconf_pad,
            android_joypad_name(autoconf_pad),
            android_joypad.ident);
   }

   engine_handle_dpad = engine_handle_dpad_default;
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

static bool android_joypad_button(unsigned port_num, uint16_t joykey)
{
   android_input_t *android = (android_input_t*)driver.input_data;

   if (!android || port_num >= MAX_PADS)
      return false;

   if (GET_HAT_DIR(joykey))
   {
      unsigned h = GET_HAT(joykey);
      if (h > 0)
         return false;

      switch (GET_HAT_DIR(joykey))
      {
         case HAT_LEFT_MASK:
            return android->hat_state[port_num][0] == -1;
         case HAT_RIGHT_MASK:
            return android->hat_state[port_num][0] ==  1;
         case HAT_UP_MASK:
            return android->hat_state[port_num][1] == -1;
         case HAT_DOWN_MASK:
            return android->hat_state[port_num][1] ==  1;
         default:
            return false;
      }
   }
   return joykey < LAST_KEYCODE && get_bit(android->pad_state[port_num],
         joykey);
}

static int16_t android_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   android_input_t *android = (android_input_t*)driver.input_data;
   if (!android || joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < MAX_AXIS)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < MAX_AXIS)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   val = android->analog_state[port_num][axis];

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void android_joypad_poll(void)
{
}

static bool android_joypad_query_pad(unsigned pad)
{
   android_input_t *android = (android_input_t*)driver.input_data;
   return (pad < MAX_PLAYERS && pad < android->pads_connected);
}


static void android_joypad_destroy(void)
{
}

const rarch_joypad_driver_t android_joypad = {
   android_joypad_init,
   android_joypad_query_pad,
   android_joypad_destroy,
   android_joypad_button,
   android_joypad_axis,
   android_joypad_poll,
   NULL,
   android_joypad_name,
   "android",
};
