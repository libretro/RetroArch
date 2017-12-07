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

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../retroarch.h"
#include "../../command.h"
#include "../../gfx/video_driver.h"
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

#define GAMEPAD_OFFSET  0

static const hid_driver_t *hid_driver = NULL;

static uint64_t pad_state[MAX_PADS];
static uint8_t pad_type[MAX_PADS-1] = {WIIUINPUT_TYPE_NONE, WIIUINPUT_TYPE_NONE, WIIUINPUT_TYPE_NONE, WIIUINPUT_TYPE_NONE};

/* 3 axis - one for touch/future IR support? */
static int16_t analog_state[MAX_PADS][3][2];
static bool wiiu_pad_inited = false;

static const char* wiiu_joypad_name(unsigned pad)
{
   if (pad > MAX_PADS) return "N/A";

   if (pad == GAMEPAD_OFFSET)
      return "WIIU Gamepad";

   if (pad < MAX_PADS)
   {
      switch (pad_type[pad-1])
      {
         case WIIUINPUT_TYPE_PRO_CONTROLLER:
            return "WIIU Pro Controller";

         case WIIUINPUT_TYPE_WIIMOTE:
            return "Wiimote Controller";

         case WIIUINPUT_TYPE_NUNCHUK:
            return "Nunchuk Controller";

         case WIIUINPUT_TYPE_CLASSIC_CONTROLLER:
            return "Classic Controller";

         case WIIUINPUT_TYPE_NONE:
         default:
            return "N/A";
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

static void wiiu_joypad_get_buttons(unsigned port_num, retro_bits_t *state)
{
	if (port_num < MAX_PADS)
   {
		BITS_COPY16_PTR( state, pad_state[port_num] );
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}

static int16_t wiiu_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int    val  = 0;
   int    axis = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 6)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 6)
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

      //For position data; just return the unmodified value
      case 4:
         return analog_state[port_num][2][0];

      case 5:
         return analog_state[port_num][2][1];
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static int16_t scaleTP(int16_t oldMin, int16_t oldMax, int16_t newMin, int16_t newMax, int16_t val) {
   int32_t oldRange = oldMax - oldMin;
   int32_t newRange = newMax - newMin;
   return (((val - oldMin) * newRange) / oldRange) + newMin;
}

static void wiiu_joypad_poll(void)
{
   int i, c;
   VPADStatus vpad;
   VPADReadError vpadError;
   #if defined(ENABLE_CONTROLLER_PATCHER)
      int result;
   #endif

   VPADRead(0, &vpad, 1, &vpadError);

   if (!vpadError)
   {
      pad_state[0] = vpad.hold & VPAD_MASK_BUTTONS; /* buttons only */
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT]  [RETRO_DEVICE_ID_ANALOG_X] = vpad.leftStick.x  * 0x7FF0;
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT]  [RETRO_DEVICE_ID_ANALOG_Y] = vpad.leftStick.y  * 0x7FF0;
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_X] = vpad.rightStick.x * 0x7FF0;
      analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_Y] = vpad.rightStick.y * 0x7FF0;

      /* You can only call VPADData once every loop, else the second one
         won't get any data. Thus; I had to hack touch support into the existing
         joystick API. Woo-hoo? Misplaced requests for touch axis are filtered
         out in wiiu_input.
      */
      if (vpad.tpNormal.touched && vpad.tpNormal.validity == VPAD_VALID) {
         struct video_viewport vp = {0};
         video_driver_get_viewport_info(&vp);
         VPADTouchData cal720p = {0};
         /* Calibrates data to a 720p screen, seems to clamp outer 12px */
         VPADGetTPCalibratedPoint(0, &cal720p, &(vpad.tpNormal));
         /* Recalibrate to match video driver's coordinate system */
         VPADTouchData calNative = {0};
         calNative.x = scaleTP(12, 1268, 0, vp.full_width, cal720p.x);
         calNative.y = scaleTP(12, 708, 0, vp.full_height, cal720p.y);
         /* Clamp to actual game image */
         VPADTouchData calClamped = calNative;
         bool touchClamped = false;
         if (calClamped.x < vp.x) {
            calClamped.x = vp.x;
            touchClamped = true;
         } else if (calClamped.x > vp.x + vp.width) {
            calClamped.x = vp.x + vp.width;
            touchClamped = true;
         }
         if (calClamped.y < vp.y) {
            calClamped.y = vp.y;
            touchClamped = true;
         } else if (calClamped.y > vp.y + vp.height) {
            calClamped.y = vp.y + vp.height;
            touchClamped = true;
         }
         /* Calibrate to libretro spec and save as axis 2 (idx 4,5) */
         analog_state[0][2][0] = scaleTP(vp.x, vp.x + vp.width, -0x7fff, 0x7fff, calClamped.x);
         analog_state[0][2][1] = scaleTP(vp.y, vp.y + vp.height, -0x7fff, 0x7fff, calClamped.y);

         /* Emulating a button (#19) for touch; lets people assign it to menu
            for that traditional RetroArch Wii U feel */
         if (!touchClamped) {
            pad_state[0] |= VPAD_BUTTON_TOUCH;
         } else {
            pad_state[0] &= ~VPAD_BUTTON_TOUCH;
         }
      } else {
         /* This is probably 0 anyway */
         pad_state[0] &= ~VPAD_BUTTON_TOUCH;
      }

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
   hid_driver = input_hid_init_first();
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
   if(hid_driver) {
     hid_driver->free(hid_driver_get_data());
     hid_driver_reset_data();
     hid_driver = NULL;
   }
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
