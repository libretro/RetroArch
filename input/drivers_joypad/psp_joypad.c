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
#include "../include/input_pad_view.h"

#include "../../tasks/tasks_internal.h"

#include "../../configuration.h"

#include <defines/psp_defines.h>

#if defined(PSP)
/* The SDK header this driver's PSP path takes SceCtrlData, the button
 * bits and the sceCtrl calls from. The VITA path includes its own
 * below; without this one the file compiled only because griffin
 * happens to include psp_input.c, which does, before it. */
#include <pspctrl.h>
#endif

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
/* each pad as its poll left it, lent to the readers (input_pad_view.h):
 * the buttons as RetroPad ids, axes 0-3 the sticks' x and y, 4 and 5
 * the Vita's L2 and R2 */
static input_pad_view_t psp_pads[DEFAULT_MAX_PADS];

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
            NULL, NULL,
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
   return input_pad_view_button(&psp_pads[port], joykey);
}

static void psp_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (port < DEFAULT_MAX_PADS)
      *state = psp_pads[port].buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t psp_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_axis(&psp_pads[port], joyaxis);
}

static int16_t psp_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   uint16_t port_idx = joypad_info->joy_idx;
   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_state(&psp_pads[port_idx], joypad_info,
         joykeys, joyaxes);
}

/* Read in the poll alone: the same until the next */
static const input_pad_view_t *psp_joypad_view(unsigned port)
{
   return (port < DEFAULT_MAX_PADS) ? &psp_pads[port] : NULL;
}

static void psp_joypad_poll(void)
{
   unsigned player;
   unsigned players_count      = DEFAULT_MAX_PADS;
#if defined(VITA)
   bool input_backtouch_enable = input_config_get_backtouch_enable();
   bool input_backtouch_toggle = input_config_get_backtouch_toggle();
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
                  NULL, NULL,
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

   for (player = 0; player < players_count; player++)
   {
      unsigned j;
      int32_t ret;
      SceCtrlData state_tmp;
      uint64_t bits       = 0;
      input_pad_view_t *v = &psp_pads[player];
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
      ret = CtrlPeekBufferPositive(p, &state_tmp, 1);

      INPUT_PAD_VIEW_SET_BITS64(v, 0);
      for (j = 0; j < 6; j++)
         v->axes[j] = 0;

      if (ret < 0)
         continue;
#if defined(VITA)
      /* The system keyboard (psp_input.c) is modal and reads the pad
       * itself; the buttons that drive it must not also drive the
       * menu behind it. */
      if (input_driver_native_keyboard_shown())
         continue;
      /* The touch panels stand in for L2/R2/L3/R3 in-game only.  In
       * the menu the front panel is the pointer and the rear one is
       * where the hands rest, and a trigger pressed from either is a
       * scroll action nobody asked for. */
      if (sceKernelGetModelForCDialog() == SCE_KERNEL_MODEL_VITA
         && input_backtouch_enable
#ifdef HAVE_MENU
         && !menu_driver_alive()
#endif
         )
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

      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_START) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SELECT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
#if defined(VITA)
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
      bits |= (STATE_BUTTON(state_tmp) & PSP_CTRL_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
      v->axes[4] = (int16_t)(STATE_ANALOGL2(state_tmp)-128) * 256;
      v->axes[5] = (int16_t)(STATE_ANALOGR2(state_tmp)-128) * 256;
#endif
      INPUT_PAD_VIEW_SET_BITS64(v, bits);

      v->axes[0] = (int16_t)(STATE_ANALOGLX(state_tmp)-128) * 256;
      v->axes[1] = (int16_t)(STATE_ANALOGLY(state_tmp)-128) * 256;
#if defined(SN_TARGET_PSP2) || defined(VITA)
      v->axes[2] = (int16_t)(STATE_ANALOGRX(state_tmp)-128) * 256;
      v->axes[3] = (int16_t)(STATE_ANALOGRY(state_tmp)-128) * 256;
#endif
      /* the sticks', not the triggers' */
      for (j = 0; j < 4; j++)
         if (v->axes[j] == -0x8000)
            v->axes[j] = -0x7fff;
   }
}

/* Whether a pad is connected, as the other joypad drivers report it:
 * the built-in controls are always there, and on the TV model a port
 * is connected when a controller is paired to it. */
static bool psp_joypad_query_pad(unsigned pad)
{
   if (pad >= DEFAULT_MAX_PADS)
      return false;
#if defined(VITA)
   if (psp2_model != SCE_KERNEL_MODEL_VITATV)
      return pad == 0;
   return curr_ctrl_info.port[pad + 1] != SCE_CTRL_TYPE_UNPAIRED;
#else
   return pad == 0;
#endif
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
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   psp_joypad_view
};
