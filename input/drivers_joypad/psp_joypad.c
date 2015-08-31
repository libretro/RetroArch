/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../input_autodetect.h"

static uint64_t pad_state;
static int16_t analog_state[1][2][2];

extern uint64_t lifecycle_state;

static const char *psp_joypad_name(unsigned pad)
{
#ifdef VITA
   return "Vita Controller";
#else
   return "PSP Controller";
#endif
}

static void psp_joypad_autodetect_add(unsigned autoconf_pad)
{
   settings_t *settings = config_get_ptr();
   autoconfig_params_t params = {{0}};

   strlcpy(settings->input.device_names[autoconf_pad],
         psp_joypad_name(autoconf_pad),
         sizeof(settings->input.device_names[autoconf_pad]));

   /* TODO - implement VID/PID? */
   params.idx = autoconf_pad;
   strlcpy(params.name, psp_joypad_name(autoconf_pad), sizeof(params.name));
   strlcpy(params.driver, psp_joypad.ident, sizeof(params.driver));
   input_config_autoconfigure_joypad(&params);
}

static bool psp_joypad_init(void *data)
{
   (void)data;

   psp_joypad_autodetect_add(0);

   return true;
}

static bool psp_joypad_button(unsigned port_num, uint16_t key)
{
   if (port_num >= MAX_PADS)
      return false;

   return (pad_state & (UINT64_C(1) << key));
}

static uint64_t psp_joypad_get_buttons(unsigned port_num)
{
   return pad_state;
}

static int16_t psp_joypad_axis(unsigned port_num, uint32_t joyaxis)
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

static void psp_joypad_poll(void)
{
   int32_t ret;
   unsigned i, j;
   SceCtrlData state_tmp;

#ifdef PSP
   sceCtrlSetSamplingCycle(0);
#endif
   sceCtrlSetSamplingMode(DEFAULT_SAMPLING_MODE);
   ret = CtrlPeekBufferPositive(0, &state_tmp, 1);
#ifdef HAVE_KERNEL_PRX
   state_tmp.Buttons = (state_tmp.Buttons&0x0000FFFF)|(read_system_buttons()&0xFFFF0000);
#endif
   (void)ret;

   analog_state[0][0][0] = analog_state[0][0][1] =
      analog_state[0][1][0] = analog_state[0][1][1] = 0;
   pad_state = 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_START) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SELECT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   pad_state |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;

   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X] = (int16_t)(STATE_ANALOGLX(state_tmp)-128) * 256;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y] = (int16_t)(STATE_ANALOGLY(state_tmp)-128) * 256;
#if defined(SN_TARGET_PSP2) || defined(VITA)
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = (int16_t)(STATE_ANALOGRX(state_tmp)-128) * 256;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = (int16_t)(STATE_ANALOGRY(state_tmp)-128) * 256;
#endif

   for (i = 0; i < 2; i++)
      for (j = 0; j < 2; j++)
         if (analog_state[0][i][j] == -0x8000)
            analog_state[0][i][j] = -0x7fff;

   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);

#ifdef HAVE_KERNEL_PRX
   if (STATE_BUTTON(state_tmp) & PSP_CTRL_NOTE)
#else
      if (
            (pad_state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L))
            && (pad_state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R))
            && (pad_state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT))
            && (pad_state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
         )
#endif
         BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
}

static bool psp_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && pad_state;
}


static void psp_joypad_destroy(void)
{
}

input_device_driver_t psp_joypad = {
   psp_joypad_init,
   psp_joypad_query_pad,
   psp_joypad_destroy,
   psp_joypad_button,
   psp_joypad_get_buttons,
   psp_joypad_axis,
   psp_joypad_poll,
   NULL,
   psp_joypad_name,
#ifdef VITA
   "vita",
#else
   "psp",
#endif
};
