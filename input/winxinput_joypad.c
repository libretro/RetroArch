/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

// Support 360 controllers on Windows.
// Said controllers do show under DInput but they have limitations in this mode;
// The triggers are combined rather than seperate and it is not possible to use
// the guide button.

// Some wrappers for other controllers also simulate xinput (as it is easier to implement)
// so this may be useful for those also.

// Note: if rumble is ever added (for psx and n64 cores), add rumble support to this
// as well as dinput.
#include "input_common.h"

#include "../general.h"
#include "../boolean.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

//#include <windows.h>

// There is no need to require the xinput headers and libs since
// we shall be dynamically loading them anyway. But first check that
// it hasn't been included anyway.
// official and mingw xinput headers have different include guards
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
      WORD  wButtons;
      BYTE  bLeftTrigger;
      BYTE  bRightTrigger;
      SHORT sThumbLX;
      SHORT sThumbLY;
      SHORT sThumbRX;
      SHORT sThumbRY;
   } XINPUT_GAMEPAD;

   typedef struct
   {
      DWORD          dwPacketNumber;
      XINPUT_GAMEPAD Gamepad;
   } XINPUT_STATE;

#endif

// Guide constant is not officially documented
#define XINPUT_GAMEPAD_GUIDE 0x0400

#ifndef ERROR_DEVICE_NOT_CONNECTED
   #define ERROR_DEVICE_NOT_CONNECTED 1167
#endif

// To load xinput1_3.dll
static HINSTANCE g_winxinput_dll;

// Function pointer, to be assigned with GetProcAddress
typedef __stdcall DWORD (*XInputGetStateEx_t)(DWORD, XINPUT_STATE*);
static XInputGetStateEx_t g_XInputGetStateEx;

// Guide button may or may not be available,
// but if it is available
static bool g_winxinput_guide_button_supported;

typedef struct
{
   XINPUT_STATE xstate;
   bool connected;
} winxinput_joypad_state;

static winxinput_joypad_state g_winxinput_states[4];

static bool winxinput_joypad_init(void)
{
   g_winxinput_dll = NULL;
   
   // Find the correct path to load the DLL from.
   // Usually this will be from the system directory,
   // but occasionally a user may wish to use a third-party
   // wrapper DLL (such as x360ce); support these by checking
   // the working directory first.
   
   // No need to check for existance as we will be checking LoadLibrary's
   // success anyway.
   TCHAR dll_path[MAX_PATH];
   strcpy(dll_path, "xinput1_3.dll");
   g_winxinput_dll = LoadLibrary(dll_path);
   if (!g_winxinput_dll)
   {
      // Loading from working dir failed, try to load from system.
      GetSystemDirectory(dll_path, sizeof(dll_path));
      strcpy(dll_path, "xinput1_3.dll");
      g_winxinput_dll = LoadLibrary(dll_path);
      
      if (!g_winxinput_dll)
      {
         RARCH_ERR("Failed to init XInput, ensure DirectX and controller drivers are up to date.\n");
         return false; // DLL does not exist or is invalid
      }
         
   }
   
   // If we get here then an xinput DLL is correctly loaded.
   // First try to load ordinal 100 (XInputGetStateEx).
   // Cast to make C++ happy.
   g_XInputGetStateEx = (XInputGetStateEx_t) GetProcAddress(g_winxinput_dll, (LPCSTR)100);
   g_winxinput_guide_button_supported = true;
   
   if (!g_XInputGetStateEx)
   {
      // no ordinal 100. (old version of x360ce perhaps). Load the ordinary XInputGetState,
      // at the cost of losing guide button support.
      g_winxinput_guide_button_supported = false;
      g_XInputGetStateEx = (XInputGetStateEx_t) GetProcAddress(g_winxinput_dll, "XInputGetState");
      if (!g_XInputGetStateEx)
      {
         RARCH_ERR("Failed to init XInput.\n");
         return false; // DLL was loaded but did not contain the correct function.
      }
   }
   
   // zero out the states
   for (unsigned i = 0; i < 4; ++i)
      ZeroMemory(&g_winxinput_states[i], sizeof(winxinput_joypad_state));

   // Do a dummy poll to check which controllers are connected.
   XINPUT_STATE dummy_state;
   for (unsigned i = 0; i < 4; ++i)
   {
      g_winxinput_states[i].connected = !(g_XInputGetStateEx(i, &dummy_state) == ERROR_DEVICE_NOT_CONNECTED);
      if (g_winxinput_states[i].connected)
         RARCH_LOG("Found XInput controller, player #%u\n", i);
   }
      
   
   return true;
}

static bool winxinput_joypad_query_pad(unsigned pad)
{
   if (pad >= 4)
      return false;
   else
      return g_winxinput_states[pad].connected;
}

static void winxinput_joypad_destroy(void)
{
   for (unsigned i = 0; i < 4; ++i)
      ZeroMemory(&g_winxinput_states[i], sizeof(winxinput_joypad_state));
      
   FreeLibrary(g_winxinput_dll);
   g_winxinput_dll = NULL;
   g_XInputGetStateEx = NULL;
}

// Buttons are provided by XInput as bits of a uint16.
// Map from button index (0..10) to mask to AND against.
// dpad is handled seperately.
// Order:
// a, b, x, y, leftbump, rightbump, back, start, leftstick, rightstick, guide.
static const WORD button_index_to_bitmap_code[] =  {
   0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0020, 0x0010,
   0x0040, 0x0080, 0x0400
};

static bool winxinput_joypad_button (unsigned port_num, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;
      
   if (!(g_winxinput_states[port_num].connected))
      return false;
   

   
   uint16_t btn_word = g_winxinput_states[port_num].xstate.Gamepad.wButtons;
   
   
   if (GET_HAT_DIR(joykey))
   {

      switch (GET_HAT_DIR(joykey))
      {
         case HAT_UP_MASK:    return btn_word & XINPUT_GAMEPAD_DPAD_UP;
         case HAT_DOWN_MASK:  return btn_word & XINPUT_GAMEPAD_DPAD_DOWN;
         case HAT_LEFT_MASK:  return btn_word & XINPUT_GAMEPAD_DPAD_LEFT;
         case HAT_RIGHT_MASK: return btn_word & XINPUT_GAMEPAD_DPAD_RIGHT;
      }

      return false; // hat requested and no hat button down
   }
   else
   {
      // non-hat button
      unsigned num_buttons = g_winxinput_guide_button_supported ? 11 : 10;
      
      if (joykey >= num_buttons)
         return false;
      else
         return btn_word & button_index_to_bitmap_code[joykey];
   }

   return false;
   
}
//#include <stdio.h>
static int16_t winxinput_joypad_axis (unsigned port_num, uint32_t joyaxis)
{
   //printf("Requested axis: %u\n", joyaxis);
   if (joyaxis == AXIS_NONE)
      return 0;
      
   if (!(g_winxinput_states[port_num].connected))
      return 0;
      
   /*switch (joyaxis)
   {
      case 0: return(g_winxinput_states[port_num].xstate.Gamepad.sThumbLX);
      case 1: return (g_winxinput_states[port_num].xstate.Gamepad.sThumbLX);
      case 2: return 0;
      case 3: return 0; //do axes now
      case 4: return 0;
      case 5: return 0;
      default: return 0;
   }*/
   
   
   int16_t val  = 0;
   int axis = -1;
   
   bool is_neg = false;
   bool is_pos = false;

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

   XINPUT_GAMEPAD* pad = &(g_winxinput_states[port_num].xstate.Gamepad);
   switch (axis)
   {
      case 0: val = pad->sThumbLX; break;
      case 1: val = pad->sThumbLY; break;
      case 2: val = pad->sThumbRX; break;
      case 3: val = pad->sThumbRY; break;
      
      case 4: val = pad->bLeftTrigger  * 32767 / 255; break;
      case 5: val = pad->bRightTrigger * 32767 / 255; break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
   
}

static void winxinput_joypad_poll(void)
{
   for (unsigned i = 0; i < 4; ++i)
      if (g_XInputGetStateEx(i, &(g_winxinput_states[i].xstate)) == ERROR_DEVICE_NOT_CONNECTED)
         g_winxinput_states[i].connected = false;
}

static const char* const XBOX_CONTROLLER_NAMES[4] = 
{
   "Xbox 360 Controller (Player 1)",
   "Xbox 360 Controller (Player 2)",
   "Xbox 360 Controller (Player 3)",
   "Xbox 360 Controller (Player 4)"
};

const char* winxinput_joypad_name (unsigned pad)
{
   if (pad > 3)
      return NULL;
   else
      return XBOX_CONTROLLER_NAMES[pad];
}


const rarch_joypad_driver_t winxinput_joypad = {
   winxinput_joypad_init,
   winxinput_joypad_query_pad,
   winxinput_joypad_destroy,
   winxinput_joypad_button,
   winxinput_joypad_axis,
   winxinput_joypad_poll,
   winxinput_joypad_name,
   "winxinput",
};

// A list of names for other (dinput) drivers to reject.
const LPCTSTR XBOX_PAD_NAMES_TO_REJECT[] = 
{
   "Controller (Gamepad for Xbox 360)",
   "Controller (XBOX 360 For Windows)",
   "Controller (Xbox 360 Wireless Receiver for Windows)",
   "Controller (Xbox wireless receiver for windows)",
   "XBOX 360 For Windows (Controller)",
   "Xbox 360 Wireless Receiver",
   "Xbox Receiver for Windows (Wireless Controller)",
   "Xbox wireless receiver for windows (Controller)",
   NULL
};


/*
typedef struct rarch_joypad_driver
{
   bool (*init)(void);
   bool (*query_pad)(unsigned);
   void (*destroy)(void);
   bool (*button)(unsigned, uint16_t);
   int16_t (*axis)(unsigned, uint32_t);
   void (*poll)(void);
   const char *(*name)(unsigned);

   const char *ident;
} rarch_joypad_driver_t;

*/