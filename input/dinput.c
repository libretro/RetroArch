/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifdef _MSC_VER
#pragma comment(lib, "dinput8")
#endif

#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "../general.h"
#include "../boolean.h"
#include "input_common.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <windowsx.h>

// Context has to be global as joypads also ride on this context.
static LPDIRECTINPUT8 g_ctx;

struct pointer_status
{
   int pointer_id;
   int pointer_x;
   int pointer_y;
   struct pointer_status *next;
};

struct dinput_input
{
   LPDIRECTINPUTDEVICE8 keyboard;
   LPDIRECTINPUTDEVICE8 mouse;
   const rarch_joypad_driver_t *joypad;
   uint8_t state[256];

   int mouse_rel_x;
   int mouse_rel_y;
   int mouse_x;
   int mouse_y;
   bool mouse_l, mouse_r, mouse_m;
   struct pointer_status pointer_head;  // dummy head for easier iteration
};

struct dinput_joypad
{
   LPDIRECTINPUTDEVICE8 joypad;
   DIJOYSTATE2 joy_state;
   char* joy_name;
};

static unsigned g_joypad_cnt;
static struct dinput_joypad g_pads[MAX_PLAYERS];

static void dinput_destroy_context(void)
{
   if (g_ctx)
   {
      IDirectInput8_Release(g_ctx);
      g_ctx = NULL;
   }
}

static bool dinput_init_context(void)
{
   if (g_ctx)
      return true;

   CoInitialize(NULL);

   // Who said we shouldn't have same call signature in a COM API? <_<
#ifdef __cplusplus
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8,
      (void**)&g_ctx, NULL)))
#else
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8,
      (void**)&g_ctx, NULL)))
#endif
   {
      RARCH_ERR("Failed to init DirectInput.\n");
      return false;
   }

   return true;
}

static void *dinput_init(void)
{
   if (!dinput_init_context())
   {
      RARCH_ERR("Failed to start DirectInput driver.\n");
      return NULL;
   }

   struct dinput_input *di = (struct dinput_input*)calloc(1, sizeof(*di));
   if (!di)
      return NULL;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, GUID_SysKeyboard, &di->keyboard, NULL)))
   {
      RARCH_ERR("Failed to create keyboard device.\n");
      di->keyboard = NULL;
   }

   if (FAILED(IDirectInput8_CreateDevice(g_ctx, GUID_SysMouse, &di->mouse, NULL)))
   {
      RARCH_ERR("Failed to create mouse device.\n");
      di->mouse = NULL;
   }
#else
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, &GUID_SysKeyboard, &di->keyboard, NULL)))
   {
      RARCH_ERR("Failed to create keyboard device.\n");
      di->keyboard = NULL;
   }
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, &GUID_SysMouse, &di->mouse, NULL)))
   {
      RARCH_ERR("Failed to create mouse device.\n");
      di->mouse = NULL;
   }
#endif

   if (di->keyboard)
   {
      IDirectInputDevice8_SetDataFormat(di->keyboard, &c_dfDIKeyboard);
      IDirectInputDevice8_SetCooperativeLevel(di->keyboard,
            (HWND)driver.video_window, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
      IDirectInputDevice8_Acquire(di->keyboard);
   }

   if (di->mouse)
   {
      IDirectInputDevice8_SetDataFormat(di->mouse, &c_dfDIMouse2);
      IDirectInputDevice8_SetCooperativeLevel(di->mouse, (HWND)driver.video_window,
            DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
      IDirectInputDevice8_Acquire(di->mouse);
   }

   input_init_keyboard_lut(rarch_key_map_dinput);
   di->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);

   return di;
}

static void dinput_poll(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;

   memset(di->state, 0, sizeof(di->state));
   if (di->keyboard)
   {
      if (FAILED(IDirectInputDevice8_GetDeviceState(di->keyboard, sizeof(di->state), di->state)))
      {
         IDirectInputDevice8_Acquire(di->keyboard);
         if (FAILED(IDirectInputDevice8_GetDeviceState(di->keyboard, sizeof(di->state), di->state)))
            memset(di->state, 0, sizeof(di->state));
      }
   }

   if (di->mouse)
   {
      DIMOUSESTATE2 mouse_state;
      memset(&mouse_state, 0, sizeof(mouse_state));

      if (FAILED(IDirectInputDevice8_GetDeviceState(di->mouse, sizeof(mouse_state), &mouse_state)))
      {
         IDirectInputDevice8_Acquire(di->mouse);
         if (FAILED(IDirectInputDevice8_GetDeviceState(di->mouse, sizeof(mouse_state), &mouse_state)))
            memset(&mouse_state, 0, sizeof(mouse_state));
      }

      di->mouse_rel_x = mouse_state.lX;
      di->mouse_rel_y = mouse_state.lY;
      di->mouse_l = mouse_state.rgbButtons[0];
      di->mouse_r = mouse_state.rgbButtons[1];
      di->mouse_m = mouse_state.rgbButtons[2];

      // No simple way to get absolute coordinates for RETRO_DEVICE_POINTER. Just use Win32 APIs.
      POINT point = {0};
      GetCursorPos(&point);
      ScreenToClient((HWND)driver.video_window, &point);
      di->mouse_x = point.x;
      di->mouse_y = point.y;
   }

   input_joypad_poll(di->joypad);
}

static bool dinput_keyboard_pressed(struct dinput_input *di, unsigned key)
{
   if (key >= RETROK_LAST)
      return false;

   unsigned sym = input_translate_rk_to_keysym((enum retro_key)key);
   return di->state[sym] & 0x80;
}

static bool dinput_is_pressed(struct dinput_input *di, const struct retro_keybind *binds,
      unsigned port, unsigned id)
{
   if (id >= RARCH_BIND_LIST_END)
      return false;

   const struct retro_keybind *bind = &binds[id];
   return dinput_keyboard_pressed(di, bind->key) || input_joypad_pressed(di->joypad, port, binds, id);
}

static int16_t dinput_pressed_analog(struct dinput_input *di,
      const struct retro_keybind *binds,
      unsigned index, unsigned id)
{
   unsigned id_minus = 0;
   unsigned id_plus  = 0;
   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   const struct retro_keybind *bind_minus = &binds[id_minus];
   const struct retro_keybind *bind_plus  = &binds[id_plus];
   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   int16_t pressed_minus = dinput_keyboard_pressed(di, bind_minus->key) ? -0x7fff : 0;
   int16_t pressed_plus = dinput_keyboard_pressed(di, bind_plus->key) ? 0x7fff : 0;
   return pressed_plus + pressed_minus;
}

static bool dinput_key_pressed(void *data, int key)
{
   return dinput_is_pressed((struct dinput_input*)data, g_settings.input.binds[0], 0, key);
}

static int16_t dinput_lightgun_state(struct dinput_input *di, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return di->mouse_rel_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return di->mouse_rel_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return di->mouse_l;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         return di->mouse_m;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         return di->mouse_r;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return di->mouse_m && di->mouse_r; 
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return di->mouse_m && di->mouse_l; 
      default:
         return 0;
   }
}

static int16_t dinput_mouse_state(struct dinput_input *di, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return di->mouse_rel_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return di->mouse_rel_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return di->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return di->mouse_r;
      default:
         return 0;
   }
}

static int16_t dinput_pointer_state(struct dinput_input *di, unsigned index, unsigned id, bool screen)
{
   int16_t res_x = 0, res_y = 0, res_screen_x = 0, res_screen_y = 0;
   unsigned num = 0;
   struct pointer_status *check_pos = di->pointer_head.next;
   while (check_pos && num < index)
   {
      num++;
      check_pos = check_pos->next;
   }
   if (!check_pos && index > 0) // index = 0 has mouse fallback
      return 0;

   int x = check_pos ? check_pos->pointer_x : di->mouse_x;
   int y = check_pos ? check_pos->pointer_y : di->mouse_y;
   bool pointer_down = check_pos ? true : di->mouse_l;

   bool valid = input_translate_coord_viewport(x, y,
         &res_x, &res_y, &res_screen_x, &res_screen_y);

   if (!valid)
      return 0;

   if (screen)
   {
      res_x = res_screen_x;
      res_y = res_screen_y;
   }

   bool inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

   if (!inside)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return res_x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return res_y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return pointer_down;
      default:
         return 0;
   }
}

static int16_t dinput_input_state(void *data,
      const struct retro_keybind **binds, unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   struct dinput_input *di = (struct dinput_input*)data;
   int16_t ret;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return dinput_is_pressed(di, binds[port], port, id);

      case RETRO_DEVICE_KEYBOARD:
         return dinput_keyboard_pressed(di, id);

      case RETRO_DEVICE_ANALOG:
         ret = dinput_pressed_analog(di, binds[port], index, id);
         if (!ret)
            ret = input_joypad_analog(di->joypad, port, index, id, g_settings.input.binds[port]);
         return ret;

      case RETRO_DEVICE_MOUSE:
         return dinput_mouse_state(di, id);

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return dinput_pointer_state(di, index, id, device == RARCH_DEVICE_POINTER_SCREEN);

      case RETRO_DEVICE_LIGHTGUN:
         return dinput_lightgun_state(di, id);

      default:
         return 0;
   }
}

// these are defined in later SDKs, thus ifdeffed
#ifndef WM_POINTERUPDATE
#define WM_POINTERUPDATE                0x0245
#endif
#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN                  0x0246
#endif
#ifndef WM_POINTERUP
#define WM_POINTERUP                    0x0247
#endif
#ifndef GET_POINTERID_WPARAM
#define GET_POINTERID_WPARAM(wParam)                (LOWORD(wParam))
#endif

// stores x/y in client coordinates
void dinput_pointer_store_pos(struct pointer_status *pointer, WPARAM lParam)
{
   POINT point;
   point.x = GET_X_LPARAM(lParam);
   point.y = GET_Y_LPARAM(lParam);
   ScreenToClient((HWND)driver.video_window, &point);
   pointer->pointer_x = point.x;
   pointer->pointer_y = point.y;
}

void dinput_add_pointer(struct dinput_input *di, struct pointer_status *new_pointer)
{
   new_pointer->next = NULL;
   struct pointer_status *insert_pos = &di->pointer_head;
   while (insert_pos->next)
      insert_pos = insert_pos->next;
   insert_pos->next = new_pointer;
}

void dinput_delete_pointer(struct dinput_input *di, int pointer_id)
{
   struct pointer_status *check_pos = &di->pointer_head;
   while (check_pos && check_pos->next)
   {
      if (check_pos->next->pointer_id == pointer_id)
      {
         struct pointer_status *to_delete = check_pos->next;
         check_pos->next = check_pos->next->next;
         free(to_delete);
      }
      check_pos = check_pos->next;
   }
}

struct pointer_status *dinput_find_pointer(struct dinput_input *di, int pointer_id)
{
   struct pointer_status *check_pos = di->pointer_head.next;
   while (check_pos)
   {
      if (check_pos->pointer_id == pointer_id)
         break;
      check_pos = check_pos->next;
   }
   return check_pos;
}

void dinput_clear_pointers(struct dinput_input *di)
{
   struct pointer_status *pointer = &di->pointer_head;
   while (pointer->next)
   {
      struct pointer_status *del = pointer->next;
      pointer->next = pointer->next->next;
      free(del);
   }
}

#ifdef __cplusplus
extern "C"
#endif
bool dinput_handle_message(void *dinput, UINT message, WPARAM wParam, LPARAM lParam)
{
   struct dinput_input *di = (struct dinput_input *)dinput;
   /* WM_POINTERDOWN arrives for each new touch event with a new id - add to list
      WM_POINTERUP arrives once the pointer is no longer down - remove from list
      WM_POINTERUPDATE arrives for both pressed and hovering pointers - ignore hovering
   */
   switch (message)
   {
      case WM_POINTERDOWN:
      {
         struct pointer_status *new_pointer = (struct pointer_status *)malloc(sizeof(struct pointer_status));
         new_pointer->pointer_id = GET_POINTERID_WPARAM(wParam);
         dinput_pointer_store_pos(new_pointer, lParam);
         dinput_add_pointer(di, new_pointer);
         return true;
      }
      case WM_POINTERUP:
      {
         int pointer_id = GET_POINTERID_WPARAM(wParam);
         dinput_delete_pointer(di, pointer_id);
         return true;
      }
      case WM_POINTERUPDATE:
      {
         int pointer_id = GET_POINTERID_WPARAM(wParam);
         struct pointer_status *pointer = dinput_find_pointer(di, pointer_id);
         if (pointer)
            dinput_pointer_store_pos(pointer, lParam);
         return true;
      }
      case WM_DEVICECHANGE:
      {
         if (di->joypad)
            di->joypad->destroy();
         di->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);
         break;
      }
   }
   return false;
}

static void dinput_free(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   LPDIRECTINPUT8 hold_ctx = g_ctx;

   if (di)
   {
      g_ctx = NULL; // Prevent a joypad driver to kill our context prematurely.
      if (di->joypad)
         di->joypad->destroy();
      g_ctx = hold_ctx;

      dinput_clear_pointers(di); // clear any leftover pointers

      if (di->keyboard)
         IDirectInputDevice8_Release(di->keyboard);

      if (di->mouse)
         IDirectInputDevice8_Release(di->mouse);

      free(di);
   }

   dinput_destroy_context();
}

static void dinput_grab_mouse(void *data, bool state)
{
   struct dinput_input *di = (struct dinput_input*)data;
   IDirectInputDevice8_Unacquire(di->mouse);
   IDirectInputDevice8_SetCooperativeLevel(di->mouse,
      (HWND)driver.video_window,
      state ?
      (DISCL_EXCLUSIVE | DISCL_FOREGROUND) :
      (DISCL_NONEXCLUSIVE | DISCL_FOREGROUND));
   IDirectInputDevice8_Acquire(di->mouse);
}

static bool dinput_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   struct dinput_input *di = (struct dinput_input*)data;
   return input_joypad_set_rumble(di->joypad, port, effect, strength);
}

static const rarch_joypad_driver_t *dinput_get_joypad_driver(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   return di->joypad;
}

static uint64_t dinput_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_LIGHTGUN);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

const input_driver_t input_dinput = {
   dinput_init,
   dinput_poll,
   dinput_input_state,
   dinput_key_pressed,
   dinput_free,
   NULL,
   NULL,
   NULL,
   dinput_get_capabilities,
   "dinput",

   dinput_grab_mouse,
   dinput_set_rumble,
   dinput_get_joypad_driver,
};

// Keep track of which pad indexes are 360 controllers
// not static, will be read in winxinput_joypad.c
// -1 = not xbox pad, otherwise 0..3
int g_xinput_pad_indexes[MAX_PLAYERS];
bool g_xinput_block_pads;

static void dinput_joypad_destroy(void)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].joypad)
      {
         IDirectInputDevice8_Unacquire(g_pads[i].joypad);
         IDirectInputDevice8_Release(g_pads[i].joypad);
      }
      
      free(g_pads[i].joy_name);
      g_pads[i].joy_name = NULL;
      *g_settings.input.device_names[i] = '\0';
   }

   g_joypad_cnt = 0;
   memset(g_pads, 0, sizeof(g_pads));

   // Can be blocked by global Dinput context.
   dinput_destroy_context();
}

static BOOL CALLBACK enum_axes_cb(const DIDEVICEOBJECTINSTANCE *inst, void *p)
{
   LPDIRECTINPUTDEVICE8 joypad = (LPDIRECTINPUTDEVICE8)p;

   DIPROPRANGE range;
   memset(&range, 0, sizeof(range));
   range.diph.dwSize = sizeof(DIPROPRANGE);
   range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   range.diph.dwHow = DIPH_BYID;
   range.diph.dwObj = inst->dwType;
   range.lMin = -0x7fff;
   range.lMax = 0x7fff;
   IDirectInputDevice8_SetProperty(joypad, DIPROP_RANGE, &range.diph);

   return DIENUM_CONTINUE;
}

// TODO: Use a better way of detecting dual XInput/DInput pads. This current method
// will not work correctly for third-party controllers or future MS pads (Xbox One?).
// An example of this is provided in the DX SDK, which advises "Enum each PNP device
// using WMI and check each device ID to see if it contains "IG_"". Unfortunately the
// example code is a horrible unsightly mess.
static const char* const XINPUT_PAD_NAMES[] = 
{
   "XBOX 360 For Windows",
   "Controller (Gamepad for Xbox 360)",
   "Controller (XBOX 360 For Windows)",
   "Controller (Xbox 360 Wireless Receiver for Windows)",
   "Controller (Xbox wireless receiver for windows)",
   "XBOX 360 For Windows (Controller)",
   "Xbox 360 Wireless Receiver",
   "Xbox 360 Wireless Controller",
   "Xbox Receiver for Windows (Wireless Controller)",
   "Xbox wireless receiver for windows (Controller)",
   "Gamepad F310 (Controller)",
   "Controller (Gamepad F310)",
   "Wireless Gamepad F710 (Controller)",
   "Controller (Batarang wired controller (XBOX))",
   "Afterglow Gamepad for Xbox 360 (Controller)"
   "Controller (Rumble Gamepad F510)",
   "Controller (Wireless Gamepad F710)",
   "Controller (Xbox 360 Wireless Receiver for Windows)",
   "Controller (Xbox wireless receiver for windows)",
   "Controller (XBOX360 GAMEPAD)",
   "MadCatz GamePad",
   "MadCatz GamePad (Controller)",
   "Controller (MadCatz GamePad)",
   "Controller (GPX Gamepad)",
   NULL
};

static bool name_is_xinput_pad(const char* name)
{
   unsigned i;
   for (i = 0; XINPUT_PAD_NAMES[i]; i++)
   {
      if (strcasecmp(name, XINPUT_PAD_NAMES[i]) == 0)
         return true;
   }

   return false;
}

// Forward declaration
static const char *dinput_joypad_name(unsigned pad);
static unsigned g_last_xinput_pad_index;

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
   (void)p;
   if (g_joypad_cnt == MAX_PLAYERS)
      return DIENUM_STOP;

   LPDIRECTINPUTDEVICE8 *pad = &g_pads[g_joypad_cnt].joypad;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, inst->guidInstance, pad, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, &inst->guidInstance, pad, NULL)))
#endif
   return DIENUM_CONTINUE;
   
   g_pads[g_joypad_cnt].joy_name = strdup(inst->tszProductName);
   
#ifdef HAVE_WINXINPUT
   bool is_xinput_pad = g_xinput_block_pads && name_is_xinput_pad(inst->tszProductName);
   
   if (is_xinput_pad)
   {
      if (g_last_xinput_pad_index < 4)
         g_xinput_pad_indexes[g_joypad_cnt] = g_last_xinput_pad_index++;
      goto enum_iteration_done;
   }
#endif

   IDirectInputDevice8_SetDataFormat(*pad, &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(*pad, (HWND)driver.video_window,
         DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(*pad, enum_axes_cb, 
         *pad, DIDFT_ABSAXIS);
         
#ifdef HAVE_WINXINPUT
   if (!is_xinput_pad)
#endif
   {
      strlcpy(g_settings.input.device_names[g_joypad_cnt], dinput_joypad_name(g_joypad_cnt), sizeof(g_settings.input.device_names[g_joypad_cnt]));
      input_config_autoconfigure_joypad(g_joypad_cnt, dinput_joypad_name(g_joypad_cnt), dinput_joypad.ident);
   }

enum_iteration_done:
   g_joypad_cnt++;
   return DIENUM_CONTINUE; 
}

static bool dinput_joypad_init(void)
{
   unsigned i;
   if (!dinput_init_context())
      return false;
   
   g_last_xinput_pad_index = 0;
   
   for (i = 0; i < MAX_PLAYERS; ++i)
   {
      g_xinput_pad_indexes[i] = -1;
      g_pads[i].joy_name = NULL;
   }

   RARCH_LOG("Enumerating DInput joypads ...\n");
   IDirectInput8_EnumDevices(g_ctx, DI8DEVCLASS_GAMECTRL,
         enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);
   RARCH_LOG("Done enumerating DInput joypads ...\n");
   return true;
}

static bool dinput_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   const struct dinput_joypad *pad = &g_pads[port_num];
   if (!pad->joypad)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      unsigned hat = GET_HAT(joykey);
      
      unsigned elems = sizeof(pad->joy_state.rgdwPOV) / sizeof(pad->joy_state.rgdwPOV[0]);
      if (hat >= elems)
         return false;

      unsigned pov = pad->joy_state.rgdwPOV[hat];

      // Magic numbers I'm not sure where originate from.
      if (pov < 36000)
      {
         switch (GET_HAT_DIR(joykey))
         {
            case HAT_UP_MASK:
               return (pov >= 31500) || (pov <= 4500);
            case HAT_RIGHT_MASK:
               return (pov >= 4500) && (pov <= 13500);
            case HAT_DOWN_MASK:
               return (pov >= 13500) && (pov <= 22500);
            case HAT_LEFT_MASK:
               return (pov >= 22500) && (pov <= 31500);
         }
      }

      return false;
   }
   else
   {
      unsigned elems = sizeof(pad->joy_state.rgbButtons) / sizeof(pad->joy_state.rgbButtons[0]);

      if (joykey < elems)
         return pad->joy_state.rgbButtons[joykey];
   }

   return false;
}

static int16_t dinput_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   const struct dinput_joypad *pad = &g_pads[port_num];
   if (!pad->joypad)
      return 0;

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) <= 5)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 5)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0: val = pad->joy_state.lX; break;
      case 1: val = pad->joy_state.lY; break;
      case 2: val = pad->joy_state.lZ; break;
      case 3: val = pad->joy_state.lRx; break;
      case 4: val = pad->joy_state.lRy; break;
      case 5: val = pad->joy_state.lRz; break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void dinput_joypad_poll(void)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      struct dinput_joypad *pad = &g_pads[i];

      if (pad->joypad && g_xinput_pad_indexes[i] < 0)
      {
         memset(&pad->joy_state, 0, sizeof(pad->joy_state));

         if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         {
            if (FAILED(IDirectInputDevice8_Acquire(pad->joypad)))
            {
               memset(&pad->joy_state, 0, sizeof(DIJOYSTATE2));
               continue;
            }

            // If this fails, something *really* bad must have happened.
            if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
            {
               memset(&pad->joy_state, 0, sizeof(DIJOYSTATE2));
               continue;
            }
         }

         IDirectInputDevice8_GetDeviceState(pad->joypad,
               sizeof(DIJOYSTATE2), &pad->joy_state);
      }
   }
}

static bool dinput_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && g_pads[pad].joypad;
}

static const char *dinput_joypad_name(unsigned pad)
{
   if (pad < MAX_PLAYERS)
      return g_pads[pad].joy_name;

   return NULL;
}

const rarch_joypad_driver_t dinput_joypad = {
   dinput_joypad_init,
   dinput_joypad_query_pad,
   dinput_joypad_destroy,
   dinput_joypad_button,
   dinput_joypad_axis,
   dinput_joypad_poll,
   NULL,
   dinput_joypad_name,
   "dinput",
};
