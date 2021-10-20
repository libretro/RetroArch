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
 * The triggers are combined rather than seperate and it is not possible to use
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
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#include "dinput_joypad.h"

#include "xinput_joypad.h"

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
static unsigned g_last_xinput_pad_idx;
static bool g_xinput_block_pads;
#ifdef HAVE_DYNAMIC
/* For xinput1_n.dll */
static dylib_t g_xinput_dll = NULL;
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
   XINPUT_GAMEPAD_RIGHT_THUMB
#ifndef _XBOX
   ,
   XINPUT_GAMEPAD_GUIDE
#endif
};

#include "dinput_joypad_inl.h"
#include "xinput_joypad_inl.h"

/* Based on SDL2's implementation. */
static bool guid_is_xinput_device(const GUID* product_guid)
{
   static const GUID common_xinput_guids[] = {
      {MAKELONG(0x28DE, 0x11FF),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}, /* Valve streaming pad */
      {MAKELONG(0x045E, 0x02A1),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}, /* Wired 360 pad */
      {MAKELONG(0x045E, 0x028E),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}  /* wireless 360 pad */
   };
   unsigned i, num_raw_devs     = 0;
   PRAWINPUTDEVICELIST raw_devs = NULL;

   /* Check for well known XInput device GUIDs,
    * thereby removing the need for the IG_ check.
    * This lets us skip RAWINPUT for popular devices.
    *
    * Also, we need to do this for the Valve Streaming Gamepad
    * because it's virtualized and doesn't show up in the device list.  */

   for (i = 0; i < ARRAY_SIZE(common_xinput_guids); ++i)
   {
      if (string_is_equal_fast(product_guid,
               &common_xinput_guids[i], sizeof(GUID)))
         return true;
   }

   /* Go through RAWINPUT (WinXP and later) to find HID devices. */
   if (!raw_devs)
   {
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
         raw_devs = NULL;
         return false;
      }
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
         raw_devs = NULL;
         return true;
      }

      if (dev_name)
         free(dev_name);
   }

   free(raw_devs);
   raw_devs = NULL;
   return false;
}

static bool dinput_joypad_get_vidpid_from_xinput_index(
      int32_t index, int32_t *vid,
      int32_t *pid, int32_t *dinput_index)
{
   int i;

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

   g_pads[g_joypad_cnt].vid = inst->guidProduct.Data1 % 0x10000;
   g_pads[g_joypad_cnt].pid = inst->guidProduct.Data1 / 0x10000;

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

   if (!is_xinput_pad)
   {
      input_autoconfigure_connect(
            g_pads[g_joypad_cnt].joy_name,
            g_pads[g_joypad_cnt].joy_friendly_name,
            dinput_joypad.ident,
            g_joypad_cnt,
            g_pads[g_joypad_cnt].vid,
            g_pads[g_joypad_cnt].pid);
   }

enum_iteration_done:
   g_joypad_cnt++;
   return DIENUM_CONTINUE;
}

static void dinput_joypad_init_hybrid(void *data)
{
   unsigned i;

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
   {
      g_xinput_states[i].xstate.dwPacketNumber        = 0;
      g_xinput_states[i].xstate.Gamepad.wButtons      = 0;
      g_xinput_states[i].xstate.Gamepad.bLeftTrigger  = 0;
      g_xinput_states[i].xstate.Gamepad.bRightTrigger = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbLX      = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbLY      = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbRX      = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbRY      = 0;
      g_xinput_states[i].connected                    = 
         !(g_XInputGetStateEx(i, &dummy_state) == ERROR_DEVICE_NOT_CONNECTED);
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

         input_autoconfigure_connect(
               name,
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

static void xinput_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < 4; ++i)
   {
      g_xinput_states[i].xstate.dwPacketNumber        = 0;
      g_xinput_states[i].xstate.Gamepad.wButtons      = 0;
      g_xinput_states[i].xstate.Gamepad.bLeftTrigger  = 0;
      g_xinput_states[i].xstate.Gamepad.bRightTrigger = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbLX      = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbLY      = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbRX      = 0;
      g_xinput_states[i].xstate.Gamepad.sThumbRY      = 0;
      g_xinput_states[i].connected                    = false;
   }

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
   dylib_close(g_xinput_dll);

   g_xinput_dll        = NULL;
#endif
   g_XInputGetStateEx  = NULL;
   g_XInputSetState    = NULL;

   dinput_joypad_destroy();

   g_xinput_block_pads = false;
}


static int32_t xinput_joypad_button(unsigned port, uint16_t joykey)
{
   int xuser                  = PAD_INDEX_TO_XUSER_INDEX(port);
   xinput_joypad_state *state = &g_xinput_states[xuser];
   uint16_t btn_word          = 0;
   if (xuser == -1)
      return dinput_joypad_button(port, joykey);
   if (!state->connected)
      return 0;
   btn_word          = state->xstate.Gamepad.wButtons;
   return xinput_joypad_button_state(xuser, btn_word, port, joykey);
}

static int16_t xinput_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int xuser                  = PAD_INDEX_TO_XUSER_INDEX(port);
   xinput_joypad_state *state = &g_xinput_states[xuser];
   XINPUT_GAMEPAD *pad        = &state->xstate.Gamepad;
   if (xuser == -1)
      return dinput_joypad_axis(port, joyaxis);
   if (!state->connected)
      return 0;
   return xinput_joypad_axis_state(pad, port, joyaxis);
}

static int16_t xinput_joypad_state_func(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   uint16_t btn_word;
   int16_t ret                = 0;
   uint16_t port_idx          = joypad_info->joy_idx;
   int xuser                  = PAD_INDEX_TO_XUSER_INDEX(port_idx);
   xinput_joypad_state *state = &g_xinput_states[xuser];
   XINPUT_GAMEPAD *pad        = &state->xstate.Gamepad;
   if (xuser == -1)
      return dinput_joypad_state(joypad_info, binds, port_idx);
   if (!state->connected)
      return 0;
   btn_word                   = state->xstate.Gamepad.wButtons;

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
            ((float)abs(xinput_joypad_axis_state(pad, port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void xinput_joypad_poll(void)
{
   unsigned i;

   for (i = 0; i < 4; ++i)
   {
      xinput_joypad_state *state = &g_xinput_states[i];
      DWORD status               = g_XInputGetStateEx(i, &state->xstate);
      bool success               = (status == ERROR_SUCCESS);
      bool new_connected         = (status != ERROR_DEVICE_NOT_CONNECTED);
      if (new_connected != state->connected)
      {
         state->connected = new_connected;
         if (!success)
            input_autoconfigure_disconnect(i, xinput_joypad_name(i));
      }
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;
      HRESULT ret;
      struct dinput_joypad_data *pad  = &g_pads[i];
      bool                    polled  = g_xinput_pad_indexes[i] < 0;
      if (!polled || !pad || !pad->joypad)
         continue;

      pad->joy_state.lX               = 0;
      pad->joy_state.lY               = 0;
      pad->joy_state.lRx              = 0;
      pad->joy_state.lRy              = 0;
      pad->joy_state.lRz              = 0;
      pad->joy_state.rglSlider[0]     = 0;
      pad->joy_state.rglSlider[1]     = 0;
      pad->joy_state.rgdwPOV[0]       = 0;
      pad->joy_state.rgdwPOV[1]       = 0;
      pad->joy_state.rgdwPOV[2]       = 0;
      pad->joy_state.rgdwPOV[3]       = 0;
      for (j = 0; j < 128; j++)
         pad->joy_state.rgbButtons[j] = 0;

      pad->joy_state.lVX              = 0;
      pad->joy_state.lVY              = 0;
      pad->joy_state.lVZ              = 0;
      pad->joy_state.lVRx             = 0;
      pad->joy_state.lVRy             = 0;
      pad->joy_state.lVRz             = 0;
      pad->joy_state.rglVSlider[0]    = 0;
      pad->joy_state.rglVSlider[1]    = 0;
      pad->joy_state.lAX              = 0;
      pad->joy_state.lAY              = 0;
      pad->joy_state.lAZ              = 0;
      pad->joy_state.lARx             = 0;
      pad->joy_state.lARy             = 0;
      pad->joy_state.lARz             = 0;
      pad->joy_state.rglASlider[0]    = 0;
      pad->joy_state.rglASlider[1]    = 0;
      pad->joy_state.lFX              = 0;
      pad->joy_state.lFY              = 0;
      pad->joy_state.lFZ              = 0;
      pad->joy_state.lFRx             = 0;
      pad->joy_state.lFRy             = 0;
      pad->joy_state.lFRz             = 0;
      pad->joy_state.rglFSlider[0]    = 0;
      pad->joy_state.rglFSlider[1]    = 0;

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
   int xuser = PAD_INDEX_TO_XUSER_INDEX(pad);

   if (xuser == -1)
      return dinput_joypad_set_rumble(pad, effect, strength);

   /* Consider the low frequency (left) motor the "strong" one. */
   if (effect == RETRO_RUMBLE_STRONG)
      g_xinput_rumble_states[xuser].wLeftMotorSpeed  = strength;
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
   xinput_joypad_state_func,
   NULL,
   xinput_joypad_axis,
   xinput_joypad_poll,
   xinput_joypad_rumble,
   NULL,
   xinput_joypad_name,
   "xinput",
};
