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

static const char *ps3_joypad_name(unsigned pad)
{
   return g_settings.input.device_names[pad];
}

static bool ps3_joypad_init(void)
{
   unsigned autoconf_pad;

   for (autoconf_pad = 0; autoconf_pad < MAX_PLAYERS; autoconf_pad++)
   {
      strlcpy(g_settings.input.device_names[autoconf_pad],
            "SixAxis Controller",
            sizeof(g_settings.input.device_names[autoconf_pad]));
      input_config_autoconfigure_joypad(autoconf_pad,
            ps3_joypad_name(autoconf_pad),
            ps3_joypad.ident);
   }

   return true;
}

static bool ps3_joypad_button(unsigned port_num, uint16_t joykey)
{
   ps3_input_t *ps3 = (ps3_input_t*)driver.input_data;

   if (!ps3 || port_num >= MAX_PADS)
      return false;

   return ps3->pad_state[port_num] & (1ULL << joykey);
}

static int16_t ps3_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   ps3_input_t *ps3 = (ps3_input_t*)driver.input_data;

   if (!ps3 || joyaxis == AXIS_NONE || port_num >= MAX_PADS)
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
         val = ps3->analog_state[port_num][0][0];
         break;
      case 1:
         val = ps3->analog_state[port_num][0][1];
         break;
      case 2:
         val = ps3->analog_state[port_num][1][0];
         break;
      case 3:
         val = ps3->analog_state[port_num][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void ps3_joypad_poll(void)
{
}

static bool ps3_joypad_query_pad(unsigned pad)
{
   ps3_input_t *ps3 = (ps3_input_t*)driver.input_data;
   return (ps3 && pad < MAX_PLAYERS && ps3->pad_state[pad]);
}


static void ps3_joypad_destroy(void)
{
}

rarch_joypad_driver_t ps3_joypad = {
   ps3_joypad_init,
   ps3_joypad_query_pad,
   ps3_joypad_destroy,
   ps3_joypad_button,
   ps3_joypad_axis,
   ps3_joypad_poll,
   NULL,
   ps3_joypad_name,
   "ps3",
};
