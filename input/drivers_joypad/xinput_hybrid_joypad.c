/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2015 - pinumbernumber
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

/* Support 360 controllers on Windows.
 * Said controllers do show under DInput but they have limitations in this mode;
 * The triggers are combined rather than separate and it is not possible to use
 * the guide button.
 *
 * Some wrappers for other controllers also simulate xinput (as it is easier to implement)
 * so this may be useful for those also.
 **/

/* Specialized version of xinput_joypad.c,
 * has both DirectInput and XInput codepaths */

/* TODO/FIXME - integrate dinput_joypad into this version */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifndef __DINPUT_JOYPAD_H
#define __DINPUT_JOYPAD_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#define WIN32_LEAN_AND_MEAN
#include <dinput.h>

/* For DIJOYSTATE2 struct, rgbButtons will always have 128 elements */
#define ARRAY_SIZE_RGB_BUTTONS 128

/* DirectInput POV value indicating the hat is centred (no direction pressed).
 * rgdwPOV[] returns this sentinel when the hat is released. */
#define DINPUT_POV_CENTERED 0xFFFFFFFFu

RETRO_BEGIN_DECLS

struct dinput_joypad_data
{
   LPDIRECTINPUTDEVICE8 joypad;
   DIJOYSTATE2          joy_state;
   char                *joy_name;
   char                *joy_friendly_name;
   int32_t              vid;
   int32_t              pid;
   LPDIRECTINPUTEFFECT  rumble_iface[2];
   DIEFFECT             rumble_props;
};

RETRO_END_DECLS

#endif

#ifndef __XINPUT_JOYPAD_H
#define __XINPUT_JOYPAD_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#if defined(__WINRT__)
#include <Xinput.h>
#endif

/* Check if the definitions do not already exist.
 * Official and mingw xinput headers have different include guards.
 * Windows 10 API version doesn't have an include guard at all and just uses #pragma once instead
 */
#if ((!_XINPUT_H_) && (!__WINE_XINPUT_H)) && !defined(__WINRT__) && !defined(_XBOX)

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

typedef struct
{
   uint16_t wButtons;
   uint8_t  bLeftTrigger;
   uint8_t  bRightTrigger;
   int16_t  sThumbLX;
   int16_t  sThumbLY;
   int16_t  sThumbRX;
   int16_t  sThumbRY;
} XINPUT_GAMEPAD;

typedef struct
{
   uint32_t       dwPacketNumber;
   XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

typedef struct
{
   uint16_t wLeftMotorSpeed;
   uint16_t wRightMotorSpeed;
} XINPUT_VIBRATION;

#endif

/* Guide constant is not officially documented. */
#define XINPUT_GAMEPAD_GUIDE 0x0400

#ifndef ERROR_DEVICE_NOT_CONNECTED
#define ERROR_DEVICE_NOT_CONNECTED 1167
#endif

#endif

/* Due to 360 pads showing up under both XInput and DirectInput,
 * and since we are going to have to pass through unhandled
 * joypad numbers to DirectInput, a slightly ugly
 * hack is required here. dinput_joypad_init will fill this.
 *
 * For each pad index, the appropriate entry will be set to -1 if it is not
 * a 360 pad, or the correct XInput user number (0..3 inclusive) if it is.
 */
typedef struct
{
   XINPUT_STATE xstate;
   bool         connected;
} xinput_joypad_state;

/* TODO/FIXME - static globals */
static int g_xinput_pad_indexes[MAX_USERS];
static unsigned g_last_xinput_pad_idx       = 0;
static bool g_xinput_block_pads             = false;
#if defined(HAVE_DYLIB) && !defined(__WINRT__)
/* For xinput1_n.dll */
static dylib_t g_xinput_dll                 = NULL;
#endif
/* Function pointer, to be assigned with dylib_proc */
typedef uint32_t (__stdcall *XInputGetStateEx_t)(uint32_t, XINPUT_STATE*);
typedef uint32_t (__stdcall *XInputSetState_t)(uint32_t, XINPUT_VIBRATION*);
/* Guide button may or may not be available */
static bool g_xinput_guide_button_supported = false;
static unsigned g_xinput_num_buttons        = 0;
static XInputSetState_t g_XInputSetState;
static XInputGetStateEx_t g_XInputGetStateEx;
#ifdef _XBOX1
static XINPUT_FEEDBACK     g_xinput_rumble_states[4];
#else
static XINPUT_VIBRATION    g_xinput_rumble_states[4];
#endif
static xinput_joypad_state g_xinput_states[4];
static bool xinput_active_port[4] = {0};

static unsigned xinput_hotplug_index = 0;
static unsigned xinput_poll_counter = 0;

/* Buttons are provided by XInput as bits of a uint16.
 * Map from rarch button index (0..10) to a mask to
 * bitwise-& the buttons against.
 * dpad is handled separately. */
static const uint16_t button_index_to_bitmap_code[] =  {
   XINPUT_GAMEPAD_A,
   XINPUT_GAMEPAD_B,
   XINPUT_GAMEPAD_X,
   XINPUT_GAMEPAD_Y,
   XINPUT_GAMEPAD_LEFT_SHOULDER,
   XINPUT_GAMEPAD_RIGHT_SHOULDER,
   XINPUT_GAMEPAD_START,
   XINPUT_GAMEPAD_BACK,
   XINPUT_GAMEPAD_LEFT_THUMB,
   XINPUT_GAMEPAD_RIGHT_THUMB
#ifndef _XBOX
   ,
   XINPUT_GAMEPAD_GUIDE
#endif
};

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
   int16_t  threshold_int;

   (void)port; /* joy_idx from joypad_info is authoritative */

   port_idx = joypad_info->joy_idx;
   if (port_idx >= MAX_USERS)
      return 0;

   pad = &g_pads[port_idx];
   if (!pad->joypad)
      return 0;

   ret = 0;
   threshold_int = (int16_t)(joypad_info->axis_threshold * 0x8000);
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
            && abs(dinput_joypad_axis_state(pad, joyaxis))
                  > threshold_int)
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

#ifndef __XINPUT_JOYPAD_INL_H
#define __XINPUT_JOYPAD_INL_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
static bool load_xinput_dll(void)
{
   const char *version = "1.4";
   /* Find the correct path to load the DLL from.
    * Usually this will be from the system directory,
    * but occasionally a user may wish to use a third-party
    * wrapper DLL (such as x360ce); support these by checking
    * the working directory first.
    *
    * No need to check for existence as we will be checking dylib_load's
    * success anyway.
    */

   g_xinput_dll = dylib_load("xinput1_4.dll");
   if (!g_xinput_dll)
   {
      g_xinput_dll = dylib_load("xinput1_3.dll");
      version = "1.3";
   }

   if (!g_xinput_dll)
   {
      RARCH_ERR("[XInput] Failed to load XInput. Ensure DirectX and controller drivers are up to date.\n");
      return false;
   }

   RARCH_LOG("[XInput] Found XInput v%s.\n", version);
   return true;
}
#endif

static int32_t xinput_joypad_button_state(
      unsigned xuser, uint16_t btn_word,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir  = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      switch (hat_dir)
      {
         case HAT_UP_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_UP);
         case HAT_DOWN_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_DOWN);
         case HAT_LEFT_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_LEFT);
         case HAT_RIGHT_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_RIGHT);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < g_xinput_num_buttons)
      return (btn_word & button_index_to_bitmap_code[joykey]);
   return 0;
}

static int16_t xinput_joypad_axis_state(
      XINPUT_GAMEPAD *pad,
      unsigned port, uint32_t joyaxis)
{
   int16_t val         = 0;
   int     axis        = -1;
   bool is_neg         = false;
   bool is_pos         = false;
   /* triggers (axes 4,5) cannot be negative */
   if (AXIS_NEG_GET(joyaxis) <= 3)
   {
      axis             = AXIS_NEG_GET(joyaxis);
      is_neg           = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 5)
   {
      axis             = AXIS_POS_GET(joyaxis);
      is_pos           = true;
   }
   else
      return 0;

   switch (axis)
   {
      case 0:
         val = pad->sThumbLX;
         break;
      case 1:
         val = pad->sThumbLY;
         break;
      case 2:
         val = pad->sThumbRX;
         break;
      case 3:
         val = pad->sThumbRY;
         break;
      case 4:
         val = pad->bLeftTrigger  * 32767 / 255;
         break; /* map 0..255 to 0..32767 */
      case 5:
         val = pad->bRightTrigger * 32767 / 255;
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   /* Clamp to avoid overflow error. */
   else if (val == -32768)
      return -32767;
   return val;
}

#endif

/* Based on SDL2's implementation. */
static bool guid_is_xinput_device(const GUID* product_guid)
{
   static const GUID common_xinput_guids[] = {
      {MAKELONG(0x28DE, 0x11FF),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}, /* Valve streaming pad */
      {MAKELONG(0x045E, 0x02A1),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}, /* Wired 360 pad */
      {MAKELONG(0x045E, 0x028E),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}  /* wireless 360 pad */
   };
   size_t i;
   unsigned num_raw_devs        = 0;
   PRAWINPUTDEVICELIST raw_devs = NULL;

   /* Check for well known XInput device GUIDs,
    * thereby removing the need for the IG_ check.
    * This lets us skip RAWINPUT for popular devices.
    *
    * Also, we need to do this for the Valve Streaming Gamepad
    * because it's virtualized and doesn't show up in the device list.  */

   for (i = 0; i < ARRAY_SIZE(common_xinput_guids); ++i)
   {
      if (memcmp(product_guid,
               &common_xinput_guids[i], sizeof(GUID)) == 0)
         return true;
   }

   /* Go through RAWINPUT (WinXP and later) to find HID devices. */
   if ((GetRawInputDeviceList(NULL, &num_raw_devs,
               sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) || (!num_raw_devs))
      return false;

   raw_devs = (PRAWINPUTDEVICELIST)
      malloc(sizeof(RAWINPUTDEVICELIST) * num_raw_devs);
   if (!raw_devs)
      return false;

   if (GetRawInputDeviceList(raw_devs, &num_raw_devs,
            sizeof(RAWINPUTDEVICELIST)) == (UINT)-1)
   {
      free(raw_devs);
      return false;
   }

   for (i = 0; i < num_raw_devs; i++)
   {
      RID_DEVICE_INFO rdi;
      char *dev_name  = NULL;
      UINT rdi_size   = sizeof(rdi);
      UINT name_size  = 0;

      rdi.cbSize      = rdi_size;

      /*
       * Step 1 -
       * Check if device type is HID
       * Step 2 -
       * Query size of name
       * Step 3 -
       * Allocate string holding ID of device
       * Step 4 -
       * query ID of device
       * Step 5 -
       * Check if the device ID contains "IG_".
       * If it does, then it's an XInput device
       * This information can not be found from DirectInput
       */
      if (
               (raw_devs[i].dwType == RIM_TYPEHID)                    /* 1 */
            && (GetRawInputDeviceInfoA(raw_devs[i].hDevice,
                RIDI_DEVICEINFO, &rdi, &rdi_size) != ((UINT)-1))
            && (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId)
             == ((LONG)product_guid->Data1))
            && (GetRawInputDeviceInfoA(raw_devs[i].hDevice,
                RIDI_DEVICENAME, NULL, &name_size) != ((UINT)-1))     /* 2 */
            && ((dev_name = (char*)malloc(name_size)) != NULL)        /* 3 */
            && (GetRawInputDeviceInfoA(raw_devs[i].hDevice,
                RIDI_DEVICENAME, dev_name, &name_size) != ((UINT)-1)) /* 4 */
            && (strstr(dev_name, "IG_"))                              /* 5 */
         )
      {
         free(dev_name);
         free(raw_devs);
         return true;
      }

      if (dev_name)
         free(dev_name);
   }

   free(raw_devs);
   return false;
}

static bool dinput_joypad_get_vidpid_from_xinput_index(
      int32_t index, int32_t *vid,
      int32_t *pid, int32_t *dinput_index)
{
   size_t i;

   for (i = 0; i < ARRAY_SIZE(g_xinput_pad_indexes); i++)
   {
      /* Found XInput pad? */
      if (index == g_xinput_pad_indexes[i])
      {
         *vid          = g_pads[i].vid;
         *pid          = g_pads[i].pid;
         *dinput_index = i;
         return true;
      }
   }

   return false;
}

static BOOL CALLBACK enum_joypad_cb_hybrid(
      const DIDEVICEINSTANCE *inst, void *p)
{
   bool is_xinput_pad;
   LPDIRECTINPUTDEVICE8 *pad = NULL;
   if (g_joypad_cnt == MAX_USERS)
      return DIENUM_STOP;

   while (!g_xinput_states[g_last_xinput_pad_idx].connected && g_last_xinput_pad_idx < 3)
   {
      g_last_xinput_pad_idx++;
   }

   pad = &g_pads[g_joypad_cnt].joypad;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_ctx, inst->guidInstance, pad, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_ctx, &inst->guidInstance, pad, NULL)))
#endif
      return DIENUM_CONTINUE;

   g_pads[g_joypad_cnt].joy_name          =
      strdup((const char*)inst->tszProductName);
   g_pads[g_joypad_cnt].joy_friendly_name =
      strdup((const char*)inst->tszInstanceName);

   /* there may be more useful info in the GUID,
    * so leave this here for a while */
#if 0
   printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
   inst->guidProduct.Data1,
   inst->guidProduct.Data2,
   inst->guidProduct.Data3,
   inst->guidProduct.Data4[0],
   inst->guidProduct.Data4[1],
   inst->guidProduct.Data4[2],
   inst->guidProduct.Data4[3],
   inst->guidProduct.Data4[4],
   inst->guidProduct.Data4[5],
   inst->guidProduct.Data4[6],
   inst->guidProduct.Data4[7]);
#endif

   g_pads[g_joypad_cnt].vid = inst->guidProduct.Data1 & 0xFFFF;
   g_pads[g_joypad_cnt].pid = inst->guidProduct.Data1 >> 16;

   is_xinput_pad            =    g_xinput_block_pads
                              && guid_is_xinput_device(&inst->guidProduct);

   if (is_xinput_pad)
   {
      if (g_last_xinput_pad_idx < 4)
         g_xinput_pad_indexes[g_joypad_cnt] = g_last_xinput_pad_idx++;
      goto enum_iteration_done;
   }

   /* Set data format to simple joystick */
   IDirectInputDevice8_SetDataFormat(*pad, &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(*pad,
         (HWND)video_driver_window_get(),
         DISCL_EXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(*pad, enum_axes_cb,
         *pad, DIDFT_ABSAXIS);

   dinput_create_rumble_effects(&g_pads[g_joypad_cnt]);

   input_autoconfigure_connect(
         g_pads[g_joypad_cnt].joy_name,
         g_pads[g_joypad_cnt].joy_friendly_name,
         NULL,
         dinput_joypad.ident,
         g_joypad_cnt,
         g_pads[g_joypad_cnt].vid,
         g_pads[g_joypad_cnt].pid);

enum_iteration_done:
   g_joypad_cnt++;
   return DIENUM_CONTINUE;
}

static void dinput_joypad_init_hybrid(void *data)
{
   int i;

   g_last_xinput_pad_idx = 0;

   for (i = 0; i < MAX_USERS; ++i)
   {
      g_xinput_pad_indexes[i]     = -1;
      g_pads[i].joy_name          = NULL;
      g_pads[i].joy_friendly_name = NULL;
   }

   IDirectInput8_EnumDevices(g_dinput_ctx, DI8DEVCLASS_GAMECTRL,
         enum_joypad_cb_hybrid, NULL, DIEDFL_ATTACHEDONLY);
}

#define PAD_INDEX_TO_XUSER_INDEX(pad) (g_xinput_pad_indexes[(pad)])

static const char *xinput_joypad_name(unsigned pad)
{
   /* On platforms with dinput support, we are able
    * to get a name from the device itself */
   return dinput_joypad_name(pad);
}

static void *xinput_joypad_init(void *data)
{
   int i, j;
   XINPUT_STATE dummy_state;

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
   if (!g_xinput_dll)
      if (!load_xinput_dll())
         goto error;

   /* If we get here then an xinput DLL is correctly loaded.
    * First try to load ordinal 100 (XInputGetStateEx).
    */
   g_XInputGetStateEx = (XInputGetStateEx_t)dylib_proc(
         g_xinput_dll, (const char*)100);
#elif defined(__WINRT__)
   /* XInputGetStateEx is not available on WinRT */
   g_XInputGetStateEx = NULL;
#else
   g_XInputGetStateEx = (XInputGetStateEx_t)XInputGetStateEx;
#endif
   g_xinput_guide_button_supported = true;

   if (!g_XInputGetStateEx)
   {
      /* no ordinal 100. (Presumably a wrapper.) Load the ordinary
       * XInputGetState, at the cost of losing guide button support.
       */
      g_xinput_guide_button_supported = false;
#if defined(HAVE_DYLIB) && !defined(__WINRT__)
      g_XInputGetStateEx = (XInputGetStateEx_t)dylib_proc(
            g_xinput_dll, "XInputGetState");
#else
      g_XInputGetStateEx = (XInputGetStateEx_t)XInputGetState;
#endif

      if (!g_XInputGetStateEx)
      {
         RARCH_ERR("[XInput] Failed to init: DLL is invalid or corrupt.\n");
#if defined(HAVE_DYLIB) && !defined(__WINRT__)
         dylib_close(g_xinput_dll);
#endif
         /* DLL was loaded but did not contain the correct function. */
         goto error;
      }
      RARCH_WARN("[XInput] No guide button support.\n");
   }

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
   g_XInputSetState = (XInputSetState_t)dylib_proc(
         g_xinput_dll, "XInputSetState");
#else
   g_XInputSetState = (XInputSetState_t)XInputSetState;
#endif
   if (!g_XInputSetState)
   {
      RARCH_ERR("[XInput] Failed to init: DLL is invalid or corrupt.\n");
#if defined(HAVE_DYLIB) && !defined(__WINRT__)
      dylib_close(g_xinput_dll);
#endif
      goto error; /* DLL was loaded but did not contain the correct function. */
   }

   /* Zero out the states. */
   for (i = 0; i < 4; ++i)
   {
      memset(&g_xinput_states[i].xstate, 0, sizeof(XINPUT_STATE));
      g_xinput_states[i].connected                    =
         !(g_XInputGetStateEx(i, &dummy_state) == ERROR_DEVICE_NOT_CONNECTED);
   }

   for (i = 0; i < 4; ++i)
      xinput_active_port[i] = false;

   if (  (!g_xinput_states[0].connected) &&
         (!g_xinput_states[1].connected) &&
         (!g_xinput_states[2].connected) &&
         (!g_xinput_states[3].connected))
#ifdef __WINRT__
      goto succeeded;
#else
      goto error;
#endif

   g_xinput_block_pads = true;

   /* We're going to have to be buddies with dinput if we want to be able
    * to use XInput and non-XInput controllers together. */
   if (!dinput_init_context())
   {
      g_xinput_block_pads = false;
      goto error;
   }

   dinput_joypad_init_hybrid(data);

   for (j = 0; j < MAX_USERS; j++)
   {
      const char *name = xinput_joypad_name(j);

      if (PAD_INDEX_TO_XUSER_INDEX(j) > -1)
      {
         int32_t vid          = 0;
         int32_t pid          = 0;
         int32_t dinput_index = 0;
         bool success         = dinput_joypad_get_vidpid_from_xinput_index((int32_t)PAD_INDEX_TO_XUSER_INDEX(j), (int32_t*)&vid, (int32_t*)&pid,
			 (int32_t*)&dinput_index);

         /* On success, found VID/PID from dinput index */
         if (!success)
            continue;

         input_autoconfigure_connect(
               name,
               NULL, NULL,
               xinput_joypad.ident,
               j,
               vid,
               pid);
      }
   }

   for (i = 0; i < MAX_USERS; ++i)
   {
      int xuser = PAD_INDEX_TO_XUSER_INDEX(i);
      if (xuser >= 0 && xuser < 4)
         xinput_active_port[xuser] = true;
   }

#ifdef __WINRT__
succeeded:
#endif
   /* non-hat button. */
   g_xinput_num_buttons = g_xinput_guide_button_supported ? 11 : 10;

   return (void*)-1;

error:
   /* non-hat button. */
   g_xinput_num_buttons = g_xinput_guide_button_supported ? 11 : 10;

   return NULL;
}

static bool xinput_joypad_query_pad(unsigned pad)
{
   int xuser = PAD_INDEX_TO_XUSER_INDEX(pad);
   if (xuser > -1)
      return g_xinput_states[xuser].connected;
   return dinput_joypad_query_pad(pad);
}

static int32_t xinput_joypad_button(unsigned port, uint16_t joykey)
{
   int xuser                  = PAD_INDEX_TO_XUSER_INDEX(port);
   xinput_joypad_state *state;
   uint16_t btn_word;
   if (xuser == -1)
      return dinput_joypad_button(port, joykey);
   state = &g_xinput_states[xuser];
   if (!state->connected)
      return 0;
   btn_word = state->xstate.Gamepad.wButtons;
   return xinput_joypad_button_state(xuser, btn_word, port, joykey);
}

static int16_t xinput_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int xuser                  = PAD_INDEX_TO_XUSER_INDEX(port);
   xinput_joypad_state *state;
   XINPUT_GAMEPAD *pad;
   if (xuser == -1)
      return dinput_joypad_axis(port, joyaxis);
   state = &g_xinput_states[xuser];
   pad   = &state->xstate.Gamepad;
   if (!state->connected)
      return 0;
   return xinput_joypad_axis_state(pad, port, joyaxis);
}

static int16_t xinput_joypad_state_func(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int i;
   uint16_t btn_word;
   int16_t ret                = 0;
   uint16_t port_idx          = joypad_info->joy_idx;
   int xuser                  = PAD_INDEX_TO_XUSER_INDEX(port_idx);
   xinput_joypad_state *state;
   XINPUT_GAMEPAD *pad;
   int16_t threshold_int;
   if (xuser == -1)
      return dinput_joypad_state(joypad_info, binds, port_idx);
   state = &g_xinput_states[xuser];
   pad   = &state->xstate.Gamepad;
   if (!state->connected)
      return 0;
   btn_word                   = state->xstate.Gamepad.wButtons;
   threshold_int              = (int16_t)(joypad_info->axis_threshold * 0x8000);

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN
            && xinput_joypad_button_state(
               xuser, btn_word, port_idx, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            abs(xinput_joypad_axis_state(pad, port_idx, joyaxis))
             > threshold_int)
         ret |= (1 << i);
   }

   return ret;
}

static void xinput_joypad_poll(void)
{
   int i;
#ifdef __WINRT__
   bool has_active_ports = false;
#endif
   
   /* Hotplugging detection: scanning one port at a time every few frames,
    * to avoid polling overload and framerate drops. */
   xinput_poll_counter++;
   if (xinput_poll_counter >= 15)
   {
      xinput_poll_counter = 0;
      if (!xinput_active_port[xinput_hotplug_index])
      {
         XINPUT_STATE tmp_state;
         DWORD result = g_XInputGetStateEx(xinput_hotplug_index, &tmp_state);
         if (result == ERROR_SUCCESS)
         {
            const char *name = xinput_joypad_name(xinput_hotplug_index);
            int32_t vid = 0;
            int32_t pid = 0;
            input_autoconfigure_connect(
               name,
               NULL, NULL,
               xinput_joypad.ident,
               xinput_hotplug_index,
               vid,
               pid);

            xinput_active_port[xinput_hotplug_index] = true;
         }
      }
         xinput_hotplug_index = (xinput_hotplug_index + 1) % 4;
   }

#ifdef __WINRT__
   for (i = 0; i < 4; ++i)
   {
      if (xinput_active_port[i])
         has_active_ports = true;
   }
#endif

   for (i = 0; i < 4; ++i)
   {
      DWORD status;
      bool success, new_connected;
      xinput_joypad_state *state;
       /* On UWP, controllers may become available after initialization.
       * If no ports are currently active, we need to poll all ports
       * to catch any late arriving controllers. */
#ifdef __WINRT__
      if (!xinput_active_port[i] && has_active_ports)
         continue;
#else
      if (!xinput_active_port[i])
         continue;
#endif

      state         = &g_xinput_states[i];
      status        = g_XInputGetStateEx(i, &state->xstate);
      success       = (status == ERROR_SUCCESS);
      new_connected = (status != ERROR_DEVICE_NOT_CONNECTED);
      if (new_connected != state->connected)
      {
         state->connected = new_connected;
         if (!success)
            input_autoconfigure_disconnect(i, xinput_joypad_name(i));
      }
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      HRESULT ret;
      struct dinput_joypad_data *pad  = &g_pads[i];
      bool                    polled  = g_xinput_pad_indexes[i] < 0;
      if (!polled || !pad || !pad->joypad)
         continue;

      memset(&pad->joy_state, 0, sizeof(DIJOYSTATE2));

      /* If this fails, something *really* bad must have happened. */
      if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         if (
                  FAILED(IDirectInputDevice8_Acquire(pad->joypad))
               || FAILED(IDirectInputDevice8_Poll(pad->joypad))
            )
            continue;

      ret = IDirectInputDevice8_GetDeviceState(pad->joypad,
            sizeof(DIJOYSTATE2), &pad->joy_state);

      if (ret == DIERR_INPUTLOST || ret == DIERR_NOTACQUIRED)
         input_autoconfigure_disconnect(i, g_pads[i].joy_friendly_name);
   }
}

static bool xinput_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   XINPUT_VIBRATION *state, prev;
   int xuser = PAD_INDEX_TO_XUSER_INDEX(pad);

   if (xuser == -1)
      return dinput_joypad_set_rumble(pad, effect, strength);

   state = &g_xinput_rumble_states[xuser];
   prev  = *state;

   /* Consider the low frequency (left) motor the "strong" one. */
   if (effect == RETRO_RUMBLE_STRONG)
      state->wLeftMotorSpeed  = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      state->wRightMotorSpeed = strength;

   /* Rumble state unchanged? */
   if (   (state->wLeftMotorSpeed  == prev.wLeftMotorSpeed)
       && (state->wRightMotorSpeed == prev.wRightMotorSpeed))
      return true;

   return g_XInputSetState && (g_XInputSetState(xuser, state) == ERROR_SUCCESS);
}

static void xinput_joypad_destroy(void)
{
   int i;

   for (i = 0; i < 4; ++i)
   {
      memset(&g_xinput_states[i].xstate, 0, sizeof(XINPUT_STATE));
      g_xinput_states[i].connected                    = false;
   }

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
   dylib_close(g_xinput_dll);

   g_xinput_dll        = NULL;
#endif
   g_XInputGetStateEx  = NULL;
   g_XInputSetState    = NULL;

   dinput_joypad_destroy();

   g_xinput_block_pads = false;
}

input_device_driver_t xinput_joypad = {
   xinput_joypad_init,
   xinput_joypad_query_pad,
   xinput_joypad_destroy,
   xinput_joypad_button,
   xinput_joypad_state_func,
   NULL,
   xinput_joypad_axis,
   xinput_joypad_poll,
   xinput_joypad_rumble,
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   xinput_joypad_name,
   "xinput",
};
