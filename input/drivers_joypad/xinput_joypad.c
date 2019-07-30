/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2015 - pinumbernumber
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

/* Support 360 controllers on Windows.
 * Said controllers do show under DInput but they have limitations in this mode;
 * The triggers are combined rather than seperate and it is not possible to use
 * the guide button.
 *
 * Some wrappers for other controllers also simulate xinput (as it is easier to implement)
 * so this may be useful for those also.
 **/
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

#include "../../verbosity.h"

#ifdef HAVE_DINPUT
#include "dinput_joypad.h"
#endif

#if defined(__WINRT__)
#include <Xinput.h>
#endif

/* Check if the definitions do not already exist.
 * Official and mingw xinput headers have different include guards.
 * Windows 10 API version doesn't have an include guard at all and just uses #pragma once instead
 */
#if ((!_XINPUT_H_) && (!__WINE_XINPUT_H)) && !defined(__WINRT__)

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

#ifdef HAVE_DINPUT
/* Due to 360 pads showing up under both XInput and DirectInput,
 * and since we are going to have to pass through unhandled
 * joypad numbers to DirectInput, a slightly ugly
 * hack is required here. dinput_joypad_init will fill this.
 *
 * For each pad index, the appropriate entry will be set to -1 if it is not
 * a 360 pad, or the correct XInput user number (0..3 inclusive) if it is.
 */
extern int g_xinput_pad_indexes[MAX_USERS];
extern bool g_xinput_block_pads;
#endif

#ifdef HAVE_DYNAMIC
/* For xinput1_n.dll */
static dylib_t g_xinput_dll = NULL;
#endif

/* Function pointer, to be assigned with dylib_proc */
typedef uint32_t (__stdcall *XInputGetStateEx_t)(uint32_t, XINPUT_STATE*);
static XInputGetStateEx_t g_XInputGetStateEx;

typedef uint32_t (__stdcall *XInputSetState_t)(uint32_t, XINPUT_VIBRATION*);
static XInputSetState_t g_XInputSetState;

/* Guide button may or may not be available */
static bool g_xinput_guide_button_supported = false;
static unsigned g_xinput_num_buttons        = 0;

typedef struct
{
   XINPUT_STATE xstate;
   bool         connected;
} xinput_joypad_state;

static XINPUT_VIBRATION g_xinput_rumble_states[4];

static xinput_joypad_state g_xinput_states[4];

static INLINE int pad_index_to_xuser_index(unsigned pad)
{
#ifdef HAVE_DINPUT
   return g_xinput_pad_indexes[pad];
#else
   return pad < DEFAULT_MAX_PADS 
      && g_xinput_states[pad].connected ? pad : -1;
#endif
}

/* Generic "XInput" instead of "Xbox 360", because there are
 * some other non-xbox third party PC controllers.
 */
static const char* const XBOX_CONTROLLER_NAMES[4] =
{
   "XInput Controller (User 1)",
   "XInput Controller (User 2)",
   "XInput Controller (User 3)",
   "XInput Controller (User 4)"
};

static const char* const XBOX_ONE_CONTROLLER_NAMES[4] =
{
   "XBOX One Controller (User 1)",
   "XBOX One Controller (User 2)",
   "XBOX One Controller (User 3)",
   "XBOX One Controller (User 4)"
};

const char *xinput_joypad_name(unsigned pad)
{
   int xuser = pad_index_to_xuser_index(pad);
#ifdef HAVE_DINPUT
   /* Use the real controller name for XBOX One controllers since
      they are slightly different  */
   if (xuser < 0)
      return dinput_joypad.name(pad);

   if (strstr(dinput_joypad.name(pad), "Xbox One For Windows"))
      return XBOX_ONE_CONTROLLER_NAMES[xuser];
#endif

   if (xuser < 0)
      return NULL;

   return XBOX_CONTROLLER_NAMES[xuser];
}

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
static bool load_xinput_dll(void)
{
   const char *version = "1.4";
   /* Find the correct path to load the DLL from.
    * Usually this will be from the system directory,
    * but occasionally a user may wish to use a third-party
    * wrapper DLL (such as x360ce); support these by checking
    * the working directory first.
    *
    * No need to check for existance as we will be checking dylib_load's
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
      RARCH_ERR("[XInput]: Failed to load XInput, ensure DirectX and controller drivers are up to date.\n");
      return false;
   }

   RARCH_LOG("[XInput]: Found XInput v%s.\n", version);
   return true;
}
#endif

static bool xinput_joypad_init(void *data)
{
   unsigned i, j;
   XINPUT_STATE dummy_state;

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
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
#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
      g_XInputGetStateEx = (XInputGetStateEx_t)dylib_proc(
            g_xinput_dll, "XInputGetState");
#else
	  g_XInputGetStateEx = (XInputGetStateEx_t)XInputGetState;
#endif

      if (!g_XInputGetStateEx)
      {
         RARCH_ERR("[XInput]: Failed to init: DLL is invalid or corrupt.\n");
#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
         dylib_close(g_xinput_dll);
#endif
         /* DLL was loaded but did not contain the correct function. */
         goto error;
      }
      RARCH_WARN("[XInput]: No guide button support.\n");
   }

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
   g_XInputSetState = (XInputSetState_t)dylib_proc(
         g_xinput_dll, "XInputSetState");
#else
   g_XInputSetState = (XInputSetState_t)XInputSetState;
#endif
   if (!g_XInputSetState)
   {
      RARCH_ERR("[XInput]: Failed to init: DLL is invalid or corrupt.\n");
#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
      dylib_close(g_xinput_dll);
#endif
      goto error; /* DLL was loaded but did not contain the correct function. */
   }

   /* Zero out the states. */
   for (i = 0; i < 4; ++i)
      memset(&g_xinput_states[i], 0, sizeof(xinput_joypad_state));

   /* Do a dummy poll to check which controllers are connected. */
   for (i = 0; i < 4; ++i)
   {
      g_xinput_states[i].connected = !(g_XInputGetStateEx(i, &dummy_state) == ERROR_DEVICE_NOT_CONNECTED);
      if (g_xinput_states[i].connected)
         RARCH_LOG("[XInput]: Found controller, user #%u\n", i);
   }

   if (  (!g_xinput_states[0].connected) &&
         (!g_xinput_states[1].connected) &&
         (!g_xinput_states[2].connected) &&
         (!g_xinput_states[3].connected))
#ifdef __WINRT__
      goto succeeded;
#else
      goto error;
#endif

   RARCH_LOG("[XInput]: Pads connected: %d\n",
         g_xinput_states[0].connected +
         g_xinput_states[1].connected + 
         g_xinput_states[2].connected + 
         g_xinput_states[3].connected);

#ifdef HAVE_DINPUT
   g_xinput_block_pads = true;

   /* We're going to have to be buddies with dinput if we want to be able
    * to use XInput and non-XInput controllers together. */
   if (!dinput_joypad.init(data))
   {
      g_xinput_block_pads = false;
      goto error;
   }
#endif

   for (j = 0; j < MAX_USERS; j++)
   {
      if (xinput_joypad_name(j))
         RARCH_LOG("[XInput]: Attempting autoconf for \"%s\", user #%u\n", xinput_joypad_name(j), j);
      else
         RARCH_LOG("[XInput]: Attempting autoconf for user #%u\n", j);

      if (pad_index_to_xuser_index(j) > -1)
      {
         int32_t vid          = 0;
         int32_t pid          = 0;
#ifdef HAVE_DINPUT
         int32_t dinput_index = 0;
         bool success     = dinput_joypad_get_vidpid_from_xinput_index((int32_t)pad_index_to_xuser_index(j), (int32_t*)&vid, (int32_t*)&pid,
			 (int32_t*)&dinput_index);

         if (success)
            RARCH_LOG("[XInput]: Found VID/PID (%04X/%04X) from DINPUT index %d for \"%s\", user #%u\n",
                  vid, pid, dinput_index, xinput_joypad_name(j), j);
#endif

         input_autoconfigure_connect(
               xinput_joypad_name(j),
               NULL,
               xinput_joypad.ident,
               j,
               vid,
               pid);
      }
   }

#ifdef __WINRT__
succeeded:
#endif
   /* non-hat button. */
   g_xinput_num_buttons = g_xinput_guide_button_supported ? 11 : 10;
   return true;

error:
   /* non-hat button. */
   g_xinput_num_buttons = g_xinput_guide_button_supported ? 11 : 10;
   return false;
}

static bool xinput_joypad_query_pad(unsigned pad)
{
   int xuser = pad_index_to_xuser_index(pad);
   if (xuser > -1)
      return g_xinput_states[xuser].connected;
#ifdef HAVE_DINPUT
   return dinput_joypad.query_pad(pad);
#else
   return false;
#endif
}

static void xinput_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < 4; ++i)
      memset(&g_xinput_states[i], 0, sizeof(xinput_joypad_state));

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
   dylib_close(g_xinput_dll);

   g_xinput_dll        = NULL;
#endif
   g_XInputGetStateEx  = NULL;
   g_XInputSetState    = NULL;

#ifdef HAVE_DINPUT
   dinput_joypad.destroy();

   g_xinput_block_pads = false;
#endif
}

/* Buttons are provided by XInput as bits of a uint16.
 * Map from rarch button index (0..10) to a mask to 
 * bitwise-& the buttons against.
 * dpad is handled seperately. */
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
   XINPUT_GAMEPAD_RIGHT_THUMB,
   XINPUT_GAMEPAD_GUIDE
};

static bool xinput_joypad_button(unsigned port_num, uint16_t joykey)
{
   uint16_t btn_word    = 0;
   unsigned hat_dir     = 0;
   int xuser            = pad_index_to_xuser_index(port_num);

#ifdef HAVE_DINPUT
   if (xuser == -1)
      return dinput_joypad.button(port_num, joykey);
#endif

   if (!(g_xinput_states[xuser].connected))
      return false;

   btn_word = g_xinput_states[xuser].xstate.Gamepad.wButtons;
   hat_dir  = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      switch (hat_dir)
      {
         case HAT_UP_MASK:
            return btn_word & XINPUT_GAMEPAD_DPAD_UP;
         case HAT_DOWN_MASK:
            return btn_word & XINPUT_GAMEPAD_DPAD_DOWN;
         case HAT_LEFT_MASK:
            return btn_word & XINPUT_GAMEPAD_DPAD_LEFT;
         case HAT_RIGHT_MASK:
            return btn_word & XINPUT_GAMEPAD_DPAD_RIGHT;
      }

      return false; /* hat requested and no hat button down. */
   }

   if (joykey < g_xinput_num_buttons)
      return btn_word & button_index_to_bitmap_code[joykey];

   return false;
}

static int16_t xinput_joypad_axis (unsigned port_num, uint32_t joyaxis)
{
   int xuser;
   int16_t val         = 0;
   int     axis        = -1;
   bool is_neg         = false;
   bool is_pos         = false;
   XINPUT_GAMEPAD* pad = NULL;

   if (joyaxis == AXIS_NONE)
      return 0;

   xuser = pad_index_to_xuser_index(port_num);

#ifdef HAVE_DINPUT
   if (xuser == -1)
      return dinput_joypad.axis(port_num, joyaxis);
#endif

   if (!(g_xinput_states[xuser].connected))
      return 0;

   /* triggers (axes 4,5) cannot be negative */
   if (AXIS_NEG_GET(joyaxis) <= 3)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 5)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   pad = &(g_xinput_states[xuser].xstate.Gamepad);

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
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   /* Clamp to avoid overflow error. */
   if (val == -32768)
      val = -32767;

   return val;
}

static void xinput_joypad_poll(void)
{
   unsigned i;

   for (i = 0; i < 4; ++i)
   {
#ifdef HAVE_DINPUT
      if (g_xinput_states[i].connected)
      {
         if (g_XInputGetStateEx(i,
                  &(g_xinput_states[i].xstate))
               == ERROR_DEVICE_NOT_CONNECTED)
         {
            g_xinput_states[i].connected = false;
            input_autoconfigure_disconnect(i, xinput_joypad_name(i));
         }
      }
#else
      /* Normally, dinput handles device insertion/removal for us, but
       * since dinput is not available on UWP we have to do it ourselves */
      /* Also note that on UWP, the controllers are not available on startup
       * and are instead 'plugged in' a moment later because Microsoft reasons */
      /* TODO: This may be bad for performance? */
      bool new_connected = g_XInputGetStateEx(i, &(g_xinput_states[i].xstate)) != ERROR_DEVICE_NOT_CONNECTED;
      if (new_connected != g_xinput_states[i].connected)
      {
         if (new_connected)
         {
            /* This is kinda ugly, but it's the same thing that dinput does */
            xinput_joypad_destroy();
            xinput_joypad_init(NULL);
            return;
         }

         g_xinput_states[i].connected = new_connected;
         if (!g_xinput_states[i].connected)
            input_autoconfigure_disconnect(i, xinput_joypad_name(i));
      }
#endif
   }

#ifdef HAVE_DINPUT
   dinput_joypad.poll();
#endif
}

static bool xinput_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   int xuser = pad_index_to_xuser_index(pad);

   if (xuser == -1)
   {
#ifdef HAVE_DINPUT
      if (dinput_joypad.set_rumble)
         return dinput_joypad.set_rumble(pad, effect, strength);
#endif
      return false;
   }

   /* Consider the low frequency (left) motor the "strong" one. */
   if (effect == RETRO_RUMBLE_STRONG)
      g_xinput_rumble_states[xuser].wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      g_xinput_rumble_states[xuser].wRightMotorSpeed = strength;

   if (!g_XInputSetState)
      return false;

   return (g_XInputSetState(xuser, &g_xinput_rumble_states[xuser])
      == 0);
}

input_device_driver_t xinput_joypad = {
   xinput_joypad_init,
   xinput_joypad_query_pad,
   xinput_joypad_destroy,
   xinput_joypad_button,
   NULL,
   xinput_joypad_axis,
   xinput_joypad_poll,
   xinput_joypad_rumble,
   xinput_joypad_name,
   "xinput",
};
