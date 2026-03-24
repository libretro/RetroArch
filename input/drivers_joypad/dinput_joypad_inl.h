/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2020 - Daniel De Matteis
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

#ifndef __DINPUT_JOYPAD_INL_H
#define __DINPUT_JOYPAD_INL_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <windowsx.h>
#include <dinput.h>
#include <mmsystem.h>

/* Forward declarations */
extern struct dinput_joypad_data g_pads[MAX_USERS];
extern unsigned g_joypad_cnt;
extern LPDIRECTINPUT8 g_dinput_ctx;

void dinput_destroy_context(void);
bool dinput_init_context(void);

static void dinput_create_rumble_effects(struct dinput_joypad_data *pad)
{
   DIENVELOPE        dienv;
   DICONSTANTFORCE   dicf;
   LONG              direction  = 0;
   DWORD             axis       = DIJOFS_X;

   dicf.lMagnitude              = 0;

   dienv.dwSize                 = sizeof(DIENVELOPE);
   dienv.dwAttackLevel          = 5000;
   dienv.dwAttackTime           = 250000;
   dienv.dwFadeLevel            = 0;
   dienv.dwFadeTime             = 250000;

   pad->rumble_props.dwSize                  = sizeof(DIEFFECT);
   pad->rumble_props.dwFlags                 = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
   pad->rumble_props.dwDuration              = INFINITE;
   pad->rumble_props.dwStartDelay            = 0;
   pad->rumble_props.dwTriggerButton         = DIEB_NOTRIGGER;
   pad->rumble_props.dwTriggerRepeatInterval = 0;
   pad->rumble_props.cAxes                   = 1;
   pad->rumble_props.rgdwAxes                = &axis;
   pad->rumble_props.rglDirection            = &direction;
   pad->rumble_props.lpEnvelope              = &dienv;
   pad->rumble_props.cbTypeSpecificParams    = sizeof(DICONSTANTFORCE);
   pad->rumble_props.lpvTypeSpecificParams   = &dicf;
   pad->rumble_props.dwGain                  = 0;

   /* --- strong motor (X axis) --- */
#ifdef __cplusplus
   if (IDirectInputDevice8_CreateEffect(pad->joypad, GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[0], NULL) != DI_OK)
#else
   if (IDirectInputDevice8_CreateEffect(pad->joypad, &GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[0], NULL) != DI_OK)
#endif
      RARCH_WARN("[DInput] Strong rumble unavailable.\n");

   /* --- weak motor (Y axis) --- */
   axis = DIJOFS_Y;
#ifdef __cplusplus
   if (IDirectInputDevice8_CreateEffect(pad->joypad, GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[1], NULL) != DI_OK)
#else
   if (IDirectInputDevice8_CreateEffect(pad->joypad, &GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[1], NULL) != DI_OK)
#endif
      RARCH_WARN("[DInput] Weak rumble unavailable.\n");
}

static BOOL CALLBACK enum_axes_cb(
      const DIDEVICEOBJECTINSTANCE *inst, void *p)
{
   DIPROPRANGE          range;
   LPDIRECTINPUTDEVICE8 joypad = (LPDIRECTINPUTDEVICE8)p;

   range.diph.dwSize       = sizeof(DIPROPRANGE);
   range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   range.diph.dwHow        = DIPH_BYID;
   range.diph.dwObj        = inst->dwType;
   range.lMin              = -0x7fff;
   range.lMax              =  0x7fff;

   IDirectInputDevice8_SetProperty(joypad, DIPROP_RANGE, &range.diph);
   return DIENUM_CONTINUE;
}

static int16_t dinput_joypad_get_axis_val(
      const struct dinput_joypad_data *pad, int axis)
{
   switch (axis)
   {
      case 0: return (int16_t)pad->joy_state.lX;
      case 1: return (int16_t)pad->joy_state.lY;
      case 2: return (int16_t)pad->joy_state.lZ;
      case 3: return (int16_t)pad->joy_state.lRx;
      case 4: return (int16_t)pad->joy_state.lRy;
      case 5: return (int16_t)pad->joy_state.lRz;
      case 6: return (int16_t)pad->joy_state.rglSlider[0];
      case 7: return (int16_t)pad->joy_state.rglSlider[1];
      default: break;
   }
   return 0;
}

static int32_t dinput_joypad_button_state(
      const struct dinput_joypad_data *pad,
      uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);

      if (h < 4)
      {
         /* 0xFFFFFFFF == centred, no direction active */
         unsigned pov = pad->joy_state.rgdwPOV[h];
         if (pov == DINPUT_POV_CENTERED)
            return 0;

         switch (hat_dir)
         {
            case HAT_UP_MASK:
               return (pov == JOY_POVFORWARD)
                   || (pov == JOY_POVRIGHT  / 2)
                   || (pov == JOY_POVLEFT   + JOY_POVRIGHT / 2);

            case HAT_RIGHT_MASK:
               return (pov == JOY_POVRIGHT)
                   || (pov == JOY_POVRIGHT / 2)
                   || (pov == JOY_POVRIGHT + JOY_POVRIGHT / 2);

            case HAT_DOWN_MASK:
               return (pov == JOY_POVBACKWARD)
                   || (pov == JOY_POVRIGHT   + JOY_POVRIGHT / 2)
                   || (pov == JOY_POVBACKWARD + JOY_POVRIGHT / 2);

            case HAT_LEFT_MASK:
               return (pov == JOY_POVLEFT)
                   || (pov == JOY_POVBACKWARD + JOY_POVRIGHT / 2)
                   || (pov == JOY_POVLEFT     + JOY_POVRIGHT / 2);

            default:
               break;
         }
      }
      /* hat requested but index out of range */
      return 0;
   }

   if (joykey < ARRAY_SIZE_RGB_BUTTONS)
      return pad->joy_state.rgbButtons[joykey] ? 1 : 0;

   return 0;
}

static int16_t dinput_joypad_axis_state(
      const struct dinput_joypad_data *pad,
      uint32_t joyaxis)
{
   int     axis;
   int16_t val;

   if (AXIS_NEG_GET(joyaxis) <= 7)
   {
      axis = (int)AXIS_NEG_GET(joyaxis);
      val  = dinput_joypad_get_axis_val(pad, axis);
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) <= 7)
   {
      axis = (int)AXIS_POS_GET(joyaxis);
      val  = dinput_joypad_get_axis_val(pad, axis);
      if (val > 0)
         return val;
   }

   return 0;
}

static int32_t dinput_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= MAX_USERS)
      return 0;
   if (!g_pads[port].joypad)
      return 0;
   return dinput_joypad_button_state(&g_pads[port], joykey);
}

static int16_t dinput_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= MAX_USERS)
      return 0;
   if (!g_pads[port].joypad)
      return 0;
   return dinput_joypad_axis_state(&g_pads[port], joyaxis);
}

static int16_t dinput_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t  ret;
   uint16_t port_idx;
   const struct dinput_joypad_data *pad;

   (void)port; /* joy_idx from joypad_info is authoritative */

   port_idx = joypad_info->joy_idx;
   if (port_idx >= MAX_USERS)
      return 0;

   pad = &g_pads[port_idx];
   if (!pad->joypad)
      return 0;

   ret = 0;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey  != NO_BTN)
         ? binds[i].joykey   : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis  != AXIS_NONE)
         ? binds[i].joyaxis  : joypad_info->auto_binds[i].joyaxis;

      if (     (uint16_t)joykey != NO_BTN
            && dinput_joypad_button_state(pad, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE
            && ((float)abs(dinput_joypad_axis_state(pad, joyaxis))
                  / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool dinput_joypad_query_pad(unsigned port)
{
   return port < MAX_USERS && g_pads[port].joypad;
}

static bool dinput_joypad_set_rumble(unsigned port,
      enum retro_rumble_effect type, uint16_t strength)
{
   /* RETRO_RUMBLE_STRONG == 0, RETRO_RUMBLE_WEAK == 1 — map directly */
   int iface_idx = (type == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (port >= g_joypad_cnt || !g_pads[port].rumble_iface[iface_idx])
      return false;

   if (strength)
   {
      /* Integer approximation: avoids float/double entirely.
       * strength in [0, 65535], DI_FFNOMINALMAX = 10000.
       * Result fits in DWORD. */
      g_pads[port].rumble_props.dwGain =
            (DWORD)(((unsigned)strength * (unsigned)DI_FFNOMINALMAX) / 65535u);
      IDirectInputEffect_SetParameters(g_pads[port].rumble_iface[iface_idx],
            &g_pads[port].rumble_props, DIEP_GAIN | DIEP_START);
   }
   else
      IDirectInputEffect_Stop(g_pads[port].rumble_iface[iface_idx]);

   return true;
}

static void dinput_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (g_pads[i].joypad)
      {
         unsigned r;
         for (r = 0; r < 2; r++)
         {
            if (g_pads[i].rumble_iface[r])
            {
               IDirectInputEffect_Stop(g_pads[i].rumble_iface[r]);
               IDirectInputEffect_Release(g_pads[i].rumble_iface[r]);
            }
         }

         IDirectInputDevice8_Unacquire(g_pads[i].joypad);
         IDirectInputDevice8_Release(g_pads[i].joypad);
      }

      free(g_pads[i].joy_name);
      g_pads[i].joy_name = NULL;
      free(g_pads[i].joy_friendly_name);
      g_pads[i].joy_friendly_name = NULL;

      input_config_clear_device_name(i);
   }

   g_joypad_cnt = 0;
   memset(g_pads, 0, sizeof(g_pads));

   /* Can be blocked by global DInput context. */
   dinput_destroy_context();
}

static const char *dinput_joypad_name(unsigned port)
{
   if (port < MAX_USERS)
      return g_pads[port].joy_name;
   return NULL;
}

#endif /* __DINPUT_JOYPAD_INL_H */
