/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <wiiu/vpad.h>
#include <wiiu/kpad.h>

#include "../input_config.h"
#include "../input_driver.h"
#include "../input_joypad_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../retroarch.h"
#include "../../command.h"
#include "string.h"

#include "wiiu_dbg.h"

#ifndef MAX_PADS
#define MAX_PADS 5
#endif

#define WIIUINPUT_TYPE_WIIMOTE               0x00
#define WIIUINPUT_TYPE_NUNCHUK               0x01
#define WIIUINPUT_TYPE_CLASSIC_CONTROLLER    0x02
#define WIIUINPUT_TYPE_PRO_CONTROLLER        0x1F
#define WIIUINPUT_TYPE_NONE                  0xFD

static uint64_t pad_state[MAX_PADS];
static u8 pad_type[MAX_PADS - 1] = {WIIUINPUT_TYPE_NONE, WIIUINPUT_TYPE_NONE, WIIUINPUT_TYPE_NONE, WIIUINPUT_TYPE_NONE};
static int16_t analog_state[MAX_PADS][2][2];
extern uint64_t lifecycle_state;
static bool wiiu_pad_inited = false;



static const char* wiiu_joypad_name(unsigned pad)
{
   if (pad == 0)
      return "WIIU Gamepad";

   if (pad < MAX_PADS)
   {
      switch (pad_type[pad - 1])
      {
      case WIIUINPUT_TYPE_NONE:
         return "N/A";

      case WIIUINPUT_TYPE_PRO_CONTROLLER:
         return "WIIU Pro Controller";

      case WIIUINPUT_TYPE_WIIMOTE:
         return "Wiimote Controller";

      case WIIUINPUT_TYPE_NUNCHUK:
         return "Nunchuk Controller";

      case WIIUINPUT_TYPE_CLASSIC_CONTROLLER:
         return "Classic Controller";
      }
   }

   return "unknown";
}

static void wiiu_joypad_autodetect_add(unsigned autoconf_pad)
{
   if (!input_autoconfigure_connect(
            wiiu_joypad_name(autoconf_pad),
            NULL,
            wiiu_joypad.ident,
            autoconf_pad,
            0,
            0
         ))
      input_config_set_device_name(autoconf_pad, wiiu_joypad_name(autoconf_pad));
}

static bool wiiu_joypad_button(unsigned port_num, uint16_t key)
{
   if (port_num >= MAX_PADS)
      return false;

   return (pad_state[port_num] & (UINT64_C(1) << key));
}

static uint64_t wiiu_joypad_get_buttons(unsigned port_num)
{
   return pad_state[port_num];
}

static int16_t wiiu_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int    val  = 0;
   int    axis = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

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
      val = analog_state[port_num][0][0];
      break;

   case 1:
      val = analog_state[port_num][0][1];
      break;

   case 2:
      val = analog_state[port_num][1][0];
      break;

   case 3:
      val = analog_state[port_num][1][1];
      break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void wiiu_joypad_poll(void)
{
   int c;
   VPADStatus vpad;
   VPADReadError vpadError;

   VPADRead(0, &vpad, 1, &vpadError);

   if (!vpadError)
   {
      pad_state[0] = vpad.hold & ~0x7F800000; /* clear out emulated analog sticks */
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT]  [RETRO_DEVICE_ID_ANALOG_X] = vpad.leftStick.x  * 0x7FF0;
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT]  [RETRO_DEVICE_ID_ANALOG_Y] = vpad.leftStick.y  * 0x7FF0;
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_X] = vpad.rightStick.x * 0x7FF0;
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_Y] = vpad.rightStick.y * 0x7FF0;

      BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);

      if ((vpad.tpNormal.touched) && (vpad.tpNormal.x > 200) && (vpad.tpNormal.validity) == 0)
         BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);

      /* panic button */
      if ((vpad.hold & (VPAD_BUTTON_R | VPAD_BUTTON_L | VPAD_BUTTON_STICK_R | VPAD_BUTTON_STICK_L))
                    == (VPAD_BUTTON_R | VPAD_BUTTON_L | VPAD_BUTTON_STICK_R | VPAD_BUTTON_STICK_L))
         command_event(CMD_EVENT_QUIT, NULL);
   }

   for (c = 0; c < 4; c++)
   {
      KPADData kpad;

      if (!KPADRead(c, &kpad, 1))
         continue;

      if (pad_type[c] != kpad.device_type)
      {
         pad_type[c] = kpad.device_type;
         wiiu_joypad_autodetect_add(c + 1);
      }

      switch (kpad.device_type)
      {
      case WIIUINPUT_TYPE_WIIMOTE:
         pad_state[c + 1] = kpad.btns_h;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X]  = 0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y]  = 0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_X] = 0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_Y] = 0;
         break;

      case WIIUINPUT_TYPE_NUNCHUK:
         pad_state[c + 1] = kpad.btns_h;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X]  = kpad.nunchuck.stick_x * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y]  = kpad.nunchuck.stick_y * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_X] = 0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_Y] = 0;
         break;

      case WIIUINPUT_TYPE_PRO_CONTROLLER:
         pad_state[c + 1] = kpad.classic.btns_h & ~0x3FC0000; /* clear out emulated analog sticks */
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X]  = kpad.classic.lstick_x * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y]  = kpad.classic.lstick_y * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_X] = kpad.classic.rstick_x * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_Y] = kpad.classic.rstick_y * 0x7FF0;
         break;

      case WIIUINPUT_TYPE_CLASSIC_CONTROLLER:
         pad_state[c + 1] = kpad.classic.btns_h & ~0xFF0000; /* clear out emulated analog sticks */
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X]  = kpad.classic.lstick_x * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y]  = kpad.classic.lstick_y * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_X] = kpad.classic.rstick_x * 0x7FF0;
         analog_state[c + 1][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_Y] = kpad.classic.rstick_y * 0x7FF0;
         break;
      }
   }
}

static bool wiiu_joypad_init(void* data)
{
   wiiu_joypad_autodetect_add(0);
   wiiu_joypad_poll();
   wiiu_pad_inited = true;
   (void)data;

   return true;
}

static bool wiiu_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PADS && wiiu_pad_inited;
}

static void wiiu_joypad_destroy(void)
{
   wiiu_pad_inited = false;
}

input_device_driver_t wiiu_joypad =
{
   wiiu_joypad_init,
   wiiu_joypad_query_pad,
   wiiu_joypad_destroy,
   wiiu_joypad_button,
   wiiu_joypad_get_buttons,
   wiiu_joypad_axis,
   wiiu_joypad_poll,
   NULL,
   wiiu_joypad_name,
   "wiiu",
};
