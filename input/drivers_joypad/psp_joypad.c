/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <string.h>

#include "../../config.def.h"

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"

#include "../../configuration.h"

#include <defines/psp_defines.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#if defined(VITA)
#include <psp2/kernel/sysmem.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>

/* TODO/FIXME - static globals */
static int psp2_model;
static SceCtrlPortInfo old_ctrl_info, curr_ctrl_info;
static SceCtrlActuator actuators[DEFAULT_MAX_PADS] = {0};

#define LERP(p, f, t) ((((p * 10) * (t * 10)) / (f * 10)) / 10)
#define AREA(lx, ly, rx, ry, x, y) (lx <= x && x < rx && ly <= y && y < ry)
#define TOUCH_MAX_WIDTH 1919
#define TOUCH_MAX_HEIGHT 1087
#define SCREEN_WIDTH PSP_FB_WIDTH
#define SCREEN_HEIGHT PSP_FB_HEIGHT
#define SCREEN_HALF_WIDTH SCREEN_WIDTH / 2
#define SCREEN_HALF_HEIGHT SCREEN_HEIGHT / 2
#define NW_AREA(x, y) AREA(0, 0, SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT, (x), (y))
#define NE_AREA(x, y) AREA(SCREEN_HALF_WIDTH, 0, SCREEN_WIDTH, SCREEN_HALF_HEIGHT, (x), (y))
#define SW_AREA(x, y) AREA(0, SCREEN_HALF_HEIGHT, SCREEN_HALF_WIDTH, SCREEN_HEIGHT, (x), (y))
#define SE_AREA(x, y) AREA(SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, (x), (y))
#endif

/* TODO/FIXME - static globals */
static uint64_t pad_state[DEFAULT_MAX_PADS];
static int16_t analog_state[DEFAULT_MAX_PADS][2][2];

/* TODO/FIXME - global referenced outside */
extern uint64_t lifecycle_state;

static const char *psp_joypad_name(unsigned pad)
{
#ifdef VITA
   if (psp2_model != SCE_KERNEL_MODEL_VITATV)
      return "Vita Controller";

   switch (curr_ctrl_info.port[pad + 1])
   {
      case SCE_CTRL_TYPE_DS3:
         return "DS3 Controller";
      case SCE_CTRL_TYPE_DS4:
         return "DS4 Controller";
      default:
         break;
   }
   return "Unpaired";
#else
   return "PSP Controller";
#endif
}

static void *psp_joypad_init(void *data)
{
   unsigned i;
   unsigned players_count = DEFAULT_MAX_PADS;

#if defined(VITA)
   psp2_model = sceCtrlIsMultiControllerSupported()? SCE_KERNEL_MODEL_VITATV : SCE_KERNEL_MODEL_VITA;

   if (psp2_model != SCE_KERNEL_MODEL_VITATV)
      players_count = 1;
   if (sceKernelGetModelForCDialog() != SCE_KERNEL_MODEL_VITATV)
   {
      sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK,
            SCE_TOUCH_SAMPLING_STATE_START);
      sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,
            SCE_TOUCH_SAMPLING_STATE_START);
   }
   sceCtrlGetControllerPortInfo(&curr_ctrl_info);
   memcpy(&old_ctrl_info, &curr_ctrl_info, sizeof(SceCtrlPortInfo));
#endif

   for (i = 0; i < players_count; i++)
      input_autoconfigure_connect(
            psp_joypad_name(i),
            NULL,
            psp_joypad.ident,
            i,
            0,
            0
            );

   return (void*)-1;
}

static int32_t psp_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return pad_state[port] & (UINT64_C(1) << joykey);
}

static void psp_joypad_get_buttons(unsigned port, input_bits_t *state)
{
	if (port < DEFAULT_MAX_PADS)
   {
		BITS_COPY16_PTR( state, pad_state[port] );
	}
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t psp_joypad_axis_state(unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      int16_t val  = 0;
      int16_t axis = AXIS_NEG_GET(joyaxis);
      switch (axis)
      {
         case 0:
         case 1:
            val = analog_state[port][0][axis];
            break;
         case 2:
         case 3:
            val = analog_state[port][1][axis - 2];
            break;
      }
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      int16_t val  = 0;
      int16_t axis = AXIS_POS_GET(joyaxis);
      switch (axis)
      {
         case 0:
         case 1:
            val = analog_state[port][0][axis];
            break;
         case 2:
         case 3:
            val = analog_state[port][1][axis - 2];
            break;
      }
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t psp_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return psp_joypad_axis_state(port, joyaxis);
}

static int16_t psp_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && (pad_state[port_idx] & (UINT64_C(1) << (uint16_t)joykey))
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(psp_joypad_axis_state(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void psp_joypad_poll(void)
{
   unsigned player;
   unsigned players_count      = DEFAULT_MAX_PADS;
#if defined(VITA)
   settings_t *settings        = config_get_ptr();
   bool input_backtouch_enable = settings->bools.input_backtouch_enable;
   bool input_backtouch_toggle = settings->bools.input_backtouch_toggle;
#endif

#ifdef PSP
   sceCtrlSetSamplingCycle(0);
#endif

#ifdef VITA
   if (psp2_model != SCE_KERNEL_MODEL_VITATV)
      players_count = 1;
   else
   {
      sceCtrlGetControllerPortInfo(&curr_ctrl_info);
      for (player = 0; player < players_count; player++)
      {
         if (old_ctrl_info.port[player + 1] == curr_ctrl_info.port[player + 1])
            continue;

         if (old_ctrl_info.port[player + 1] != SCE_CTRL_TYPE_UNPAIRED &&
               curr_ctrl_info.port[player + 1] == SCE_CTRL_TYPE_UNPAIRED)
         {
            memset(&actuators[player], 0, sizeof(SceCtrlActuator));
            input_autoconfigure_disconnect(player, psp_joypad.ident);
         }

         if (old_ctrl_info.port[player + 1] == SCE_CTRL_TYPE_UNPAIRED &&
               curr_ctrl_info.port[player + 1] != SCE_CTRL_TYPE_UNPAIRED)
            input_autoconfigure_connect(
                  psp_joypad_name(player),
                  NULL,
                  psp_joypad.ident,
                  player,
                  0,
                  0
                  );
      }
      memcpy(&old_ctrl_info, &curr_ctrl_info, sizeof(SceCtrlPortInfo));
   }
#endif

   CtrlSetSamplingMode(DEFAULT_SAMPLING_MODE);

   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);

   for (player = 0; player < players_count; player++)
   {
      unsigned j, k;
      SceCtrlData state_tmp;
      unsigned i  = player;
#if defined(VITA)
      unsigned p = (psp2_model == SCE_KERNEL_MODEL_VITATV) 
         ? player + 1 : player;
      if (curr_ctrl_info.port[p] == SCE_CTRL_TYPE_UNPAIRED)
         continue;
#elif defined(SN_TARGET_PSP2)
      /* Dumb hack, but here's the explanation -
       * sceCtrlPeekBufferPositive's port parameter
       * can be 0 or 1 to read the first controller on
       * a PSTV, but HAS to be 0 for a real VITA and 2
       * for the 2nd controller on a PSTV */
      unsigned p  = (player > 0) ? player+1 : player;
#else
      unsigned p  = player;
#endif
      int32_t ret = CtrlPeekBufferPositive(p, &state_tmp, 1);

      pad_state[i] = 0;
      analog_state[i][0][0] = analog_state[i][0][1] =
         analog_state[i][1][0] = analog_state[i][1][1] = 0;

#if defined(SN_TARGET_PSP2) || defined(VITA)
      if (ret < 0)
         continue;
#endif
#if defined(VITA)
      if (sceKernelGetModelForCDialog() == SCE_KERNEL_MODEL_VITA
         && input_backtouch_enable)
      {
         unsigned i;
         SceTouchData touch_surface = {0};
         sceTouchPeek(input_backtouch_toggle
               ? SCE_TOUCH_PORT_FRONT 
               : SCE_TOUCH_PORT_BACK,
               &touch_surface, 1);

         for (i = 0; i < touch_surface.reportNum; i++)
         {
            int x = LERP(touch_surface.report[i].x,
                  TOUCH_MAX_WIDTH, SCREEN_WIDTH);
            int y = LERP(touch_surface.report[i].y,
                  TOUCH_MAX_HEIGHT, SCREEN_HEIGHT);
            if (NW_AREA(x, y))
               state_tmp.buttons |= PSP_CTRL_L2;
            if (NE_AREA(x, y))
               state_tmp.buttons |= PSP_CTRL_R2;
            if (SW_AREA(x, y))
               state_tmp.buttons |= PSP_CTRL_L3;
            if (SE_AREA(x, y))
               state_tmp.buttons |= PSP_CTRL_R3;
         }
      }
#endif

      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_START) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SELECT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
#if defined(VITA)
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
      pad_state[i] |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
#endif

      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X] = (int16_t)(STATE_ANALOGLX(state_tmp)-128) * 256;
      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y] = (int16_t)(STATE_ANALOGLY(state_tmp)-128) * 256;
#if defined(SN_TARGET_PSP2) || defined(VITA)
      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = (int16_t)(STATE_ANALOGRX(state_tmp)-128) * 256;
      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = (int16_t)(STATE_ANALOGRY(state_tmp)-128) * 256;
#endif
      for (j = 0; j < 2; j++)
         for (k = 0; k < 2; k++)
            if (analog_state[i][j][k] == -0x8000)
               analog_state[i][j][k] = -0x7fff;
   }
}

static bool psp_joypad_query_pad(unsigned pad)
{
   return pad < DEFAULT_MAX_PADS && pad_state[pad];
}

static bool psp_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
#ifdef VITA
   if (psp2_model != SCE_KERNEL_MODEL_VITATV)
      return false;
   if (pad >= DEFAULT_MAX_PADS)
      return false;
   switch (effect)
   {
      case RETRO_RUMBLE_WEAK:
         switch (curr_ctrl_info.port[pad + 1])
         {
            case SCE_CTRL_TYPE_DS3:
               actuators[pad].small = strength > 1 ? 1 : 0;
               break;
            case SCE_CTRL_TYPE_DS4:
               actuators[pad].small = LERP(strength, 0xffff, 0xff);
               break;
            default:
               actuators[pad].small = 0;
         }
         break;
      case RETRO_RUMBLE_STRONG:
         switch (curr_ctrl_info.port[pad + 1])
         {
            case SCE_CTRL_TYPE_DS3:
               actuators[pad].large = strength > 1 ? LERP(strength, 0xffff, 0xbf) + 0x40 : 0;
               break;
            case SCE_CTRL_TYPE_DS4:
               actuators[pad].large = LERP(strength, 0xffff, 0xff);
               break;
            default:
               actuators[pad].large = 0;
         }
         break;
      case RETRO_RUMBLE_DUMMY:
      default:
         break;
   }
   sceCtrlSetActuator(pad + 1, &actuators[pad]);

   return true;
#else
   return false;
#endif
}

static void psp_joypad_destroy(void)
{
}

input_device_driver_t psp_joypad = {
   psp_joypad_init,
   psp_joypad_query_pad,
   psp_joypad_destroy,
   psp_joypad_button,
   psp_joypad_state,
   psp_joypad_get_buttons,
   psp_joypad_axis,
   psp_joypad_poll,
   psp_joypad_rumble,
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   psp_joypad_name,
#ifdef VITA
   "vita",
#else
   "psp",
#endif
};
