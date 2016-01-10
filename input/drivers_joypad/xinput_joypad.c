/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2015 - pinumbernumber
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <retro_inline.h>
#include <dynamic/dylib.h>
#include <boolean.h>

#include "../input_autodetect.h"
#include "../input_config.h"

#include "../../general.h"
#include "../../verbosity.h"


/* Check if the definitions do not already exist.
 * Official and mingw xinput headers have different include guards.
 */
#if ((!_XINPUT_H_) && (!__WINE_XINPUT_H))

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

#ifndef HAVE_DINPUT
#error Cannot compile xinput without dinput.
#endif

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

/* For xinput1_n.dll */
static dylib_t g_xinput_dll;

/* Function pointer, to be assigned with dylib_proc */
typedef uint32_t (__stdcall *XInputGetStateEx_t)(uint32_t, XINPUT_STATE*);
static XInputGetStateEx_t g_XInputGetStateEx;

typedef uint32_t (__stdcall *XInputSetState_t)(uint32_t, XINPUT_VIBRATION*);
static XInputSetState_t g_XInputSetState;

/* Guide button may or may not be available */
static bool g_xinput_guide_button_supported;

typedef struct
{
   XINPUT_STATE xstate;
   bool         connected;
} xinput_joypad_state;

static XINPUT_VIBRATION g_xinput_rumble_states[4];

static xinput_joypad_state g_xinput_states[4];

static INLINE int pad_index_to_xuser_index(unsigned pad)
{
   return g_xinput_pad_indexes[pad];
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

const char *xinput_joypad_name(unsigned pad)
{
   int xuser = pad_index_to_xuser_index(pad);

   if (xuser < 0)
      return dinput_joypad.name(pad);
   /* TODO: Different name if disconnected? */
   return XBOX_CONTROLLER_NAMES[xuser];
}

static bool xinput_joypad_init(void *data)
{
   unsigned i, autoconf_pad;
   XINPUT_STATE dummy_state;
   const char *version = "1.4";
   settings_t *settings = config_get_ptr();

   (void)data;

   g_xinput_dll = NULL;

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
      RARCH_ERR("Failed to load XInput, ensure DirectX and controller drivers are up to date.\n");
      return false;
   }

   RARCH_LOG("Found XInput v%s.\n", version);

   /* If we get here then an xinput DLL is correctly loaded.
    * First try to load ordinal 100 (XInputGetStateEx).
    */
   g_XInputGetStateEx = (XInputGetStateEx_t)dylib_proc(g_xinput_dll, (const char*)100);
   g_xinput_guide_button_supported = true;

   if (!g_XInputGetStateEx)
   {
      /* no ordinal 100. (Presumably a wrapper.) Load the ordinary
       * XInputGetState, at the cost of losing guide button support.
       */
      g_xinput_guide_button_supported = false;
      g_XInputGetStateEx = (XInputGetStateEx_t)dylib_proc(g_xinput_dll, "XInputGetState");

      if (!g_XInputGetStateEx)
      {
         RARCH_ERR("Failed to init XInput: DLL is invalid or corrupt.\n");
         dylib_close(g_xinput_dll);
         return false; /* DLL was loaded but did not contain the correct function. */
      }
      RARCH_WARN("XInput: No guide button support.\n");
   }

   g_XInputSetState = (XInputSetState_t)dylib_proc(g_xinput_dll, "XInputSetState");
   if (!g_XInputSetState)
   {
      RARCH_ERR("Failed to init XInput: DLL is invalid or corrupt.\n");
      dylib_close(g_xinput_dll);
      return false; /* DLL was loaded but did not contain the correct function. */
   }

   /* Zero out the states. */
   for (i = 0; i < 4; ++i)
      memset(&g_xinput_states[i], 0, sizeof(xinput_joypad_state));

   /* Do a dummy poll to check which controllers are connected. */
   for (i = 0; i < 4; ++i)
   {
      g_xinput_states[i].connected = !(g_XInputGetStateEx(i, &dummy_state) == ERROR_DEVICE_NOT_CONNECTED);
      if (g_xinput_states[i].connected)
         RARCH_LOG("Found XInput controller, user #%u\n", i);
   }

   if ((!g_xinput_states[0].connected) &&
         (!g_xinput_states[1].connected) &&
         (!g_xinput_states[2].connected) &&
         (!g_xinput_states[3].connected))
      return false;

   g_xinput_block_pads = true;

   /* We're going to have to be buddies with dinput if we want to be able
    * to use XInput and non-XInput controllers together. */
   if (!dinput_joypad.init(data))
   {
      g_xinput_block_pads = false;
      return false;
   }

   for (autoconf_pad = 0; autoconf_pad < MAX_USERS; autoconf_pad++)
   {
      if (pad_index_to_xuser_index(autoconf_pad) > -1)
      {
         autoconfig_params_t params = {{0}};

         strlcpy(settings->input.device_names[autoconf_pad],
               xinput_joypad_name(autoconf_pad),
               sizeof(settings->input.device_names[autoconf_pad]));

         /* TODO - implement VID/PID? */
         params.idx = autoconf_pad;
         strlcpy(params.name, xinput_joypad_name(autoconf_pad), sizeof(params.name));
         strlcpy(params.driver, xinput_joypad.ident, sizeof(params.driver));
         input_config_autoconfigure_joypad(&params);
      }
   }

   return true;
}

static bool xinput_joypad_query_pad(unsigned pad)
{
   int xuser = pad_index_to_xuser_index(pad);
   if (xuser > -1)
      return g_xinput_states[xuser].connected;
   return dinput_joypad.query_pad(pad);
}

static void xinput_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < 4; ++i)
      memset(&g_xinput_states[i], 0, sizeof(xinput_joypad_state));

   dylib_close(g_xinput_dll);

   g_xinput_dll        = NULL;
   g_XInputGetStateEx  = NULL;
   g_XInputSetState    = NULL;

   dinput_joypad.destroy();

   g_xinput_block_pads = false;
}

/* Buttons are provided by XInput as bits of a uint16.
 * Map from rarch button index (0..10) to a mask to bitwise-& the buttons against.
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
   uint16_t btn_word;
   int xuser;
   unsigned num_buttons = 0;

   if (joykey == NO_BTN)
      return false;

   xuser = pad_index_to_xuser_index(port_num);
   if (xuser == -1)
      return dinput_joypad.button(port_num, joykey);

   if (!(g_xinput_states[xuser].connected))
      return false;

   btn_word = g_xinput_states[xuser].xstate.Gamepad.wButtons;

   if (GET_HAT_DIR(joykey))
   {
      switch (GET_HAT_DIR(joykey))
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

   /* non-hat button. */
   num_buttons = g_xinput_guide_button_supported ? 11 : 10;

   if (joykey < num_buttons)
      return btn_word & button_index_to_bitmap_code[joykey];

   return false;
}

static int16_t xinput_joypad_axis (unsigned port_num, uint32_t joyaxis)
{
   int xuser;
   int16_t val  = 0;
   int     axis = -1;

   bool is_neg  = false;
   bool is_pos  = false;

   if (joyaxis == AXIS_NONE)
      return 0;

   xuser = pad_index_to_xuser_index(port_num);

   if (xuser == -1)
      return dinput_joypad.axis(port_num, joyaxis);

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

   XINPUT_GAMEPAD* pad = &(g_xinput_states[xuser].xstate.Gamepad);

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
      if (g_xinput_states[i].connected)
      {
         if (g_XInputGetStateEx(i, 
                  &(g_xinput_states[i].xstate)) 
               == ERROR_DEVICE_NOT_CONNECTED)
            g_xinput_states[i].connected = false;
      }
   }

   dinput_joypad.poll();
}

static bool xinput_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   int xuser = pad_index_to_xuser_index(pad);

   if (xuser == -1)
   {
      if (dinput_joypad.set_rumble)
         return dinput_joypad.set_rumble(pad, effect, strength);
      return false;
   }

   /* Consider the low frequency (left) motor the "strong" one. */
   if (effect == RETRO_RUMBLE_STRONG)
      g_xinput_rumble_states[xuser].wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      g_xinput_rumble_states[xuser].wRightMotorSpeed = strength;

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
