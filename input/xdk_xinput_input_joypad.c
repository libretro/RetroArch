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

static const char* const XBOX_CONTROLLER_NAMES[4] =
{
   "XInput Controller (Player 1)",
   "XInput Controller (Player 2)",
   "XInput Controller (Player 3)",
   "XInput Controller (Player 4)"
};

static const char *xdk_joypad_name(unsigned pad)
{
   return g_settings.input.device_names[pad];
}

static bool xdk_joypad_init(void)
{
   unsigned autoconf_pad;

   for (autoconf_pad = 0; autoconf_pad < MAX_PLAYERS; autoconf_pad++)
   {
      strlcpy(g_settings.input.device_names[autoconf_pad],
            "XInput Controller",
            sizeof(g_settings.input.device_names[autoconf_pad]));
      input_config_autoconfigure_joypad(autoconf_pad,
            xdk_joypad_name(autoconf_pad), xdk_joypad.ident);
   }

   return true;
}

static bool xdk_joypad_button(unsigned port_num, uint16_t joykey)
{
   xdk_input_t *xdk = (xdk_input_t*)driver.input_data;

   if (port_num >= MAX_PADS)
      return false;

   return xdk->pad_state[port_num] & (1ULL << joykey);
}

static int16_t xdk_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   xdk_input_t *xdk = (xdk_input_t*)driver.input_data;
   if (joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0:
         val = xdk->analog_state[port_num][0][0];
         break;
      case 1:
         val = xdk->analog_state[port_num][0][1];
         break;
      case 2:
         val = xdk->analog_state[port_num][1][0];
         break;
      case 3:
         val = xdk->analog_state[port_num][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}


static void xdk_joypad_poll(void)
{
}

static bool xdk_joypad_query_pad(unsigned pad)
{
   xdk_input_t *xdk = (xdk_input_t*)driver.input_data;
   return pad < MAX_PLAYERS && xdk->pad_state[pad];
}

static void xdk_joypad_destroy(void)
{
}

const rarch_joypad_driver_t xdk_joypad = {
   xdk_joypad_init,
   xdk_joypad_query_pad,
   xdk_joypad_destroy,
   xdk_joypad_button,
   xdk_joypad_axis,
   xdk_joypad_poll,
   NULL,
   xdk_joypad_name,
   "xdk",
};
