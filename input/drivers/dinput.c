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

#ifdef _MSC_VER
#pragma comment(lib, "dinput8")
#endif

#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x20e
#endif

#include <dinput.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <boolean.h>

#include <windowsx.h>

#include <retro_log.h>

#include "../../general.h"
#include "../input_autodetect.h"
#include "../input_common.h"
#include "../input_joypad.h"
#include "../input_keymaps.h"

/* Keep track of which pad indexes are 360 controllers.
 * Not static, will be read in xinput_joypad.c
 * -1 = not xbox pad, otherwise 0..3
 */

int g_xinput_pad_indexes[MAX_USERS];
bool g_xinput_block_pads;

/* Context has to be global as joypads also ride on this context. */
LPDIRECTINPUT8 g_dinput_ctx;

struct pointer_status
{
   int pointer_id;
   int pointer_x;
   int pointer_y;
   struct pointer_status *next;
};

struct dinput_input
{
   bool blocked;
   LPDIRECTINPUTDEVICE8 keyboard;
   LPDIRECTINPUTDEVICE8 mouse;
   const input_device_driver_t *joypad;
   uint8_t state[256];

   int window_pos_x;
   int window_pos_y;
   int mouse_x;
   int mouse_last_x;
   int mouse_y;
   int mouse_last_y;
   bool mouse_l, mouse_r, mouse_m, mouse_wu, mouse_wd, mouse_hwu, mouse_hwd;
   struct pointer_status pointer_head;  /* dummy head for easier iteration */
};

void dinput_destroy_context(void)
{
   if (!g_dinput_ctx)
      return;

   IDirectInput8_Release(g_dinput_ctx);
   g_dinput_ctx = NULL;
}

bool dinput_init_context(void)
{
   if (g_dinput_ctx)
      return true;

   CoInitialize(NULL);

   /* Who said we shouldn't have same call signature in a COM API? <_< */
#ifdef __cplusplus
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8,
      (void**)&g_dinput_ctx, NULL)))
#else
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8,
      (void**)&g_dinput_ctx, NULL)))
#endif
   {
      RARCH_ERR("Failed to init DirectInput.\n");
      return false;
   }

   return true;
}

static void *dinput_init(void)
{
   struct dinput_input *di = NULL;
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!dinput_init_context())
   {
      RARCH_ERR("Failed to start DirectInput driver.\n");
      return NULL;
   }

   di = (struct dinput_input*)calloc(1, sizeof(*di));
   if (!di)
      return NULL;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx, GUID_SysKeyboard, &di->keyboard, NULL)))
   {
      RARCH_ERR("Failed to create keyboard device.\n");
      di->keyboard = NULL;
   }

   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx, GUID_SysMouse, &di->mouse, NULL)))
   {
      RARCH_ERR("Failed to create mouse device.\n");
      di->mouse = NULL;
   }
#else
   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx, &GUID_SysKeyboard, &di->keyboard, NULL)))
   {
      RARCH_ERR("Failed to create keyboard device.\n");
      di->keyboard = NULL;
   }
   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx, &GUID_SysMouse, &di->mouse, NULL)))
   {
      RARCH_ERR("Failed to create mouse device.\n");
      di->mouse = NULL;
   }
#endif

   if (di->keyboard)
   {
      IDirectInputDevice8_SetDataFormat(di->keyboard, &c_dfDIKeyboard);
      IDirectInputDevice8_SetCooperativeLevel(di->keyboard,
            (HWND)driver->video_window, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
      IDirectInputDevice8_Acquire(di->keyboard);
   }

   if (di->mouse)
   {
      DIDATAFORMAT c_dfDIMouse2_custom = c_dfDIMouse2;

      c_dfDIMouse2_custom.dwFlags = DIDF_ABSAXIS;
      IDirectInputDevice8_SetDataFormat(di->mouse, &c_dfDIMouse2_custom);
      IDirectInputDevice8_SetCooperativeLevel(di->mouse, (HWND)driver->video_window,
            DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
      IDirectInputDevice8_Acquire(di->mouse);
   }

   input_keymaps_init_keyboard_lut(rarch_key_map_dinput);
   di->joypad = input_joypad_init_driver(settings->input.joypad_driver, di);

   return di;
}

static void dinput_poll(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   driver_t *driver = driver_get_ptr();

   memset(di->state, 0, sizeof(di->state));
   if (di->keyboard)
   {
      if (FAILED(IDirectInputDevice8_GetDeviceState(
                  di->keyboard, sizeof(di->state), di->state)))
      {
         IDirectInputDevice8_Acquire(di->keyboard);
         if (FAILED(IDirectInputDevice8_GetDeviceState(
                     di->keyboard, sizeof(di->state), di->state)))
            memset(di->state, 0, sizeof(di->state));
      }
   }

   if (di->mouse)
   {
      DIMOUSESTATE2 mouse_state;
      memset(&mouse_state, 0, sizeof(mouse_state));

      if (FAILED(IDirectInputDevice8_GetDeviceState(
                  di->mouse, sizeof(mouse_state), &mouse_state)))
      {
         IDirectInputDevice8_Acquire(di->mouse);
         if (FAILED(IDirectInputDevice8_GetDeviceState(
                     di->mouse, sizeof(mouse_state), &mouse_state)))
            memset(&mouse_state, 0, sizeof(mouse_state));
      }

      di->mouse_last_x = di->mouse_x;
      di->mouse_last_y = di->mouse_y;

      di->mouse_x = di->window_pos_x;
      di->mouse_y = di->window_pos_y;

      di->mouse_l  = mouse_state.rgbButtons[0];
      di->mouse_r  = mouse_state.rgbButtons[1];
      di->mouse_m  = mouse_state.rgbButtons[2];

      /* No simple way to get absolute coordinates
       * for RETRO_DEVICE_POINTER. Just use Win32 APIs. */
      POINT point = {0};
      GetCursorPos(&point);
      ScreenToClient((HWND)driver->video_window, &point);
      di->mouse_x = point.x;
      di->mouse_y = point.y;
   }

   if (di->joypad)
      di->joypad->poll();
}

static bool dinput_keyboard_pressed(struct dinput_input *di, unsigned key)
{
   unsigned sym;

   if (key >= RETROK_LAST)
      return false;

   sym = input_keymaps_translate_rk_to_keysym((enum retro_key)key);
   return di->state[sym] & 0x80;
}

static bool dinput_is_pressed(struct dinput_input *di,
      const struct retro_keybind *binds,
      unsigned port, unsigned id)
{
   const struct retro_keybind *bind = &binds[id];
   if (id >= RARCH_BIND_LIST_END)
      return false;

   if (!di->blocked && dinput_keyboard_pressed(di, bind->key))
      return true;
   if (input_joypad_pressed(di->joypad, port, binds, id))
      return true;

   return false;
}

static int16_t dinput_pressed_analog(struct dinput_input *di,
      const struct retro_keybind *binds,
      unsigned idx, unsigned id)
{
   const struct retro_keybind *bind_minus, *bind_plus;
   int16_t pressed_minus = 0, pressed_plus = 0;
   unsigned id_minus = 0, id_plus = 0;

   input_conv_analog_id_to_bind_id(idx, id, &id_minus, &id_plus);

   bind_minus = &binds[id_minus];
   bind_plus  = &binds[id_plus];

   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   if (dinput_keyboard_pressed(di, bind_minus->key))
      pressed_minus = -0x7fff;
   if (dinput_keyboard_pressed(di, bind_plus->key))
      pressed_plus  = 0x7fff;

   return pressed_plus + pressed_minus;
}

static bool dinput_key_pressed(void *data, int key)
{
   settings_t *settings = config_get_ptr();
   return dinput_is_pressed((struct dinput_input*)data, settings->input.binds[0], 0, key);
}

static bool dinput_meta_key_pressed(void *data, int key)
{
   return false;
}

static int16_t dinput_lightgun_state(struct dinput_input *di, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return di->mouse_x - di->mouse_last_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return di->mouse_y - di->mouse_last_y;
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
   }

   return 0;
}

static int16_t dinput_mouse_state(struct dinput_input *di, unsigned id)
{
   int16_t state = 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return di->mouse_x - di->mouse_last_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return di->mouse_y - di->mouse_last_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return di->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return di->mouse_r;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         if (di->mouse_wu)
            state = 1;
         di->mouse_wu = false;
         return state;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         if (di->mouse_wd)
            state = 1;
         di->mouse_wd = false;
         return state;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         if (di->mouse_hwu)
            state = 1;
         di->mouse_hwu = false;
         return state;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         if (di->mouse_hwd)
            state = 1;
         di->mouse_hwd = false;
         return state;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return di->mouse_m;
   }

   return 0;
}

static int16_t dinput_mouse_state_screen(struct dinput_input *di, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return di->mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return di->mouse_y;
      default:
         break;
   }

   return dinput_mouse_state(di, id);
}

static int16_t dinput_pointer_state(struct dinput_input *di,
      unsigned idx, unsigned id, bool screen)
{
   bool pointer_down, valid, inside;
   int x, y;
   int16_t res_x = 0, res_y = 0, res_screen_x = 0, res_screen_y = 0;
   unsigned num = 0;
   struct pointer_status *check_pos = di->pointer_head.next;

   while (check_pos && num < idx)
   {
      num++;
      check_pos    = check_pos->next;
   }
   if (!check_pos && idx > 0) /* idx = 0 has mouse fallback. */
      return 0;

   x               = di->mouse_x;
   y               = di->mouse_y;
   pointer_down    = di->mouse_l;

   if (check_pos)
   {
      x            = check_pos->pointer_x;
      y            = check_pos->pointer_y;
      pointer_down = true;
   }

   valid           = input_translate_coord_viewport(x, y,
         &res_x, &res_y, &res_screen_x, &res_screen_y);

   if (!valid)
      return 0;

   if (screen)
   {
      res_x        = res_screen_x;
      res_y        = res_screen_y;
   }

   inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

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
         break;
   }

   return 0;
}

static int16_t dinput_input_state(void *data,
      const struct retro_keybind **binds, unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   int16_t ret;
   struct dinput_input *di = (struct dinput_input*)data;
   settings_t *settings    = config_get_ptr();

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return dinput_is_pressed(di, binds[port], port, id);

      case RETRO_DEVICE_KEYBOARD:
         return dinput_keyboard_pressed(di, id);

      case RETRO_DEVICE_ANALOG:
         ret = dinput_pressed_analog(di, binds[port], idx, id);
         if (!ret)
            ret = input_joypad_analog(di->joypad, port,
                  idx, id, settings->input.binds[port]);
         return ret;

      case RETRO_DEVICE_MOUSE:
         return dinput_mouse_state(di, id);

      case RARCH_DEVICE_MOUSE_SCREEN:
         return dinput_mouse_state_screen(di, id);

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return dinput_pointer_state(di, idx, id,
               device == RARCH_DEVICE_POINTER_SCREEN);

      case RETRO_DEVICE_LIGHTGUN:
         return dinput_lightgun_state(di, id);
   }

   return 0;
}

/* These are defined in later SDKs, thus ifdeffed. */

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
#define GET_POINTERID_WPARAM(wParam)   (LOWORD(wParam))
#endif

/* Stores x/y in client coordinates. */
static void dinput_pointer_store_pos(
      struct pointer_status *pointer, WPARAM lParam)
{
   POINT point;
   driver_t *driver = driver_get_ptr();

   point.x = GET_X_LPARAM(lParam);
   point.y = GET_Y_LPARAM(lParam);
   ScreenToClient((HWND)driver->video_window, &point);
   pointer->pointer_x = point.x;
   pointer->pointer_y = point.y;
}

static void dinput_add_pointer(struct dinput_input *di,
      struct pointer_status *new_pointer)
{
   struct pointer_status *insert_pos = NULL;

   new_pointer->next = NULL;
   insert_pos = &di->pointer_head;

   while (insert_pos->next)
      insert_pos = insert_pos->next;
   insert_pos->next = new_pointer;
}

static void dinput_delete_pointer(struct dinput_input *di, int pointer_id)
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

static struct pointer_status *dinput_find_pointer(
      struct dinput_input *di, int pointer_id)
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

static void dinput_clear_pointers(struct dinput_input *di)
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
   settings_t *settings    = config_get_ptr();
   /* WM_POINTERDOWN   : Arrives for each new touch event
    *                    with a new ID - add to list.
    * WM_POINTERUP     : Arrives once the pointer is no
    *                    longer down - remove from list.
    * WM_POINTERUPDATE : arrives for both pressed and
    *                    hovering pointers - ignore hovering
   */

   switch (message)
   {
      case WM_MOUSEMOVE:
	 di->window_pos_x = GET_X_LPARAM(lParam);
	 di->window_pos_y = GET_Y_LPARAM(lParam);
         break;
      case WM_POINTERDOWN:
      {
         struct pointer_status *new_pointer =
            (struct pointer_status *)malloc(sizeof(struct pointer_status));

         if (!new_pointer)
         {
            RARCH_ERR("dinput_handle_message: pointer allocation in WM_POINTERDOWN failed.\n");
            return false;
         }

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
         di->joypad = input_joypad_init_driver(settings->input.joypad_driver, di);
         break;
      }
      case WM_MOUSEWHEEL:
          {
              if (((short) HIWORD(wParam))/120 > 0)
                 di->mouse_wu = true;
              if (((short) HIWORD(wParam))/120 < 0)
                 di->mouse_wd = true;
          }
          break;
      case WM_MOUSEHWHEEL:
          {
              if (((short) HIWORD(wParam))/120 > 0)
                 di->mouse_hwu = true;
              if (((short) HIWORD(wParam))/120 < 0)
                 di->mouse_hwd = true;
          }
          break;
   }

   return false;
}

static void dinput_free(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   LPDIRECTINPUT8 hold_ctx = g_dinput_ctx;

   if (di)
   {
      /* Prevent a joypad driver to kill our context prematurely. */
      g_dinput_ctx = NULL;
      if (di->joypad)
         di->joypad->destroy();
      g_dinput_ctx = hold_ctx;

      /* Clear any leftover pointers. */
      dinput_clear_pointers(di);

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
   driver_t *driver = driver_get_ptr();

   IDirectInputDevice8_Unacquire(di->mouse);
   IDirectInputDevice8_SetCooperativeLevel(di->mouse,
      (HWND)driver->video_window,
      state ?
      (DISCL_EXCLUSIVE | DISCL_FOREGROUND) :
      (DISCL_NONEXCLUSIVE | DISCL_FOREGROUND));
   IDirectInputDevice8_Acquire(di->mouse);
}

static bool dinput_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct dinput_input *di = (struct dinput_input*)data;
   if (!di)
      return false;
   return input_joypad_set_rumble(di->joypad, port, effect, strength);
}

static const input_device_driver_t *dinput_get_joypad_driver(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   if (!di)
      return NULL;
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

static bool dinput_keyboard_mapping_is_blocked(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   if (!di)
      return false;
   return di->blocked;
}

static void dinput_keyboard_mapping_set_block(void *data, bool value)
{
   struct dinput_input *di = (struct dinput_input*)data;
   if (!di)
      return;
   di->blocked = value;
}

input_driver_t input_dinput = {
   dinput_init,
   dinput_poll,
   dinput_input_state,
   dinput_key_pressed,
   dinput_meta_key_pressed,
   dinput_free,
   NULL,
   NULL,
   dinput_get_capabilities,
   "dinput",

   dinput_grab_mouse,
   NULL,
   dinput_set_rumble,
   dinput_get_joypad_driver,
   dinput_keyboard_mapping_is_blocked,
   dinput_keyboard_mapping_set_block,
};
