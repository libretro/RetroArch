/*  RetroArch - A frontend for libretro.
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

#include <Windows.h>

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../video_driver.h"
#include "../../verbosity.h"

#define WINRAW_LOG(msg) RARCH_LOG("[WINRAW]: "msg"\n")
#define WINRAW_ERR(err) RARCH_ERR("[WINRAW]: "err"\n")

#define WINRAW_SYS_WRN(fun)\
      RARCH_WARN("[WINRAW]: "fun" failed with error %lu.\n", GetLastError())
#define WINRAW_SYS_ERR(fun)\
      RARCH_ERR("[WINRAW]: "fun" failed with error %lu.\n", GetLastError())

typedef struct
{
   uint8_t keys[256];
} winraw_keyboard_t;

typedef struct
{
   LONG x, y, dlt_x, dlt_y;
   LONG whl_u, whl_d;
   bool btn_l, btn_m, btn_r;
} winraw_mouse_t;

typedef struct
{
   winraw_keyboard_t keyboard;
   winraw_mouse_t mouse;
   const input_device_driver_t *joypad;
   HWND window;
   bool kbd_mapp_block;
   bool mouse_grab;
} winraw_input_t;

static winraw_keyboard_t *g_keyboard = NULL;
static winraw_mouse_t *g_mouse       = NULL;

static HWND winraw_create_window(WNDPROC wnd_proc)
{
   HWND wnd;
   WNDCLASSA wc = {0};

   wc.hInstance = GetModuleHandleA(NULL);

   if (!wc.hInstance)
   {
      WINRAW_SYS_ERR("GetModuleHandleA");
      return NULL;
   }

   wc.lpfnWndProc   = wnd_proc;
   wc.lpszClassName = "winraw-input";
   if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
   {
      WINRAW_SYS_ERR("RegisterClassA");
      return NULL;
   }

   wnd = CreateWindowExA(0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0,
         HWND_MESSAGE, NULL, NULL, NULL);
   if (!wnd)
   {
      WINRAW_SYS_ERR("CreateWindowExA");
      goto error;
   }

   return wnd;

error:
   UnregisterClassA(wc.lpszClassName, NULL);
   return NULL;
}

static void winraw_destroy_window(HWND wnd)
{
   BOOL r;

   if (!wnd)
      return;

   r = DestroyWindow(wnd);

   if (!r)
      WINRAW_SYS_WRN("DestroyWindow");

   r = UnregisterClassA("winraw-input", NULL);

   if (!r)
      WINRAW_SYS_WRN("UnregisterClassA");
}

static bool winraw_set_keyboard_input(HWND window)
{
   RAWINPUTDEVICE rid;
   BOOL r;

   rid.dwFlags     = window ? 0 : RIDEV_REMOVE;
   rid.hwndTarget  = window;
   rid.usUsagePage = 0x01; /* generic desktop */
   rid.usUsage     = 0x06; /* keyboard */

   r               = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

   if (!r)
   {
      WINRAW_SYS_ERR("RegisterRawInputDevices");
      return false;
   }

   return true;
}

static bool winraw_set_mouse_input(HWND window, bool grab)
{
   RAWINPUTDEVICE rid;
   BOOL r;

   if (window)
      rid.dwFlags  = grab ? RIDEV_CAPTUREMOUSE : 0;
   else
      rid.dwFlags  = RIDEV_REMOVE;

   rid.hwndTarget  = window;
   rid.usUsagePage = 0x01; /* generic desktop */
   rid.usUsage     = 0x02; /* mouse */

   r               = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

   if (!r)
   {
      WINRAW_SYS_ERR("RegisterRawInputDevices");
      return false;
   }

   return true;
}

static int16_t winraw_keyboard_state(winraw_input_t *wr, unsigned id)
{
   unsigned key;

   if (id >= RETROK_LAST)
      return 0;

   key = input_keymaps_translate_rk_to_keysym((enum retro_key)id);
   return wr->keyboard.keys[key];
}

static int16_t winraw_mouse_state(winraw_input_t *wr, bool abs, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return abs ? wr->mouse.x : wr->mouse.dlt_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return abs ? wr->mouse.y : wr->mouse.dlt_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return wr->mouse.btn_l ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return wr->mouse.btn_r ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return wr->mouse.whl_u ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return wr->mouse.whl_d ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return wr->mouse.btn_m ? 1 : 0;
   }

   return 0;
}

static int16_t winraw_joypad_state(winraw_input_t *wr,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind *binds,
      unsigned port, unsigned id)
{
   const struct retro_keybind *bind = &binds[id];

   if (!wr->kbd_mapp_block && winraw_keyboard_state(wr, bind->key))
      return 1;

   return input_joypad_pressed(wr->joypad, joypad_info, port, binds, id);
}

static LRESULT CALLBACK winraw_callback(HWND wnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
   static uint8_t data[1024];
   UINT r;
   POINT crs_pos;
   RAWINPUT *ri = (RAWINPUT*)data;
   UINT size    = sizeof(data);

   if (msg != WM_INPUT)
      return DefWindowProcA(wnd, msg, wpar, lpar);

   if (GET_RAWINPUT_CODE_WPARAM(wpar) != RIM_INPUT) /* app is in the background */
      goto end;

   r = GetRawInputData((HRAWINPUT)lpar, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER));
   if (r == (UINT)-1)
   {
      WINRAW_SYS_WRN("GetRawInputData");
      goto end;
   }

   if (ri->header.dwType == RIM_TYPEKEYBOARD)
   {
      if (ri->data.keyboard.Message == WM_KEYDOWN)
         g_keyboard->keys[ri->data.keyboard.VKey] = 1;
      else if (ri->data.keyboard.Message == WM_KEYUP)
         g_keyboard->keys[ri->data.keyboard.VKey] = 0;
   }
   else if (ri->header.dwType == RIM_TYPEMOUSE)
   {
      if (ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
      {
         g_mouse->x = ri->data.mouse.lLastX;
         g_mouse->y = ri->data.mouse.lLastY;
      }
      else if (ri->data.mouse.lLastX || ri->data.mouse.lLastY)
      {
         InterlockedExchangeAdd(&g_mouse->dlt_x, ri->data.mouse.lLastX);
         InterlockedExchangeAdd(&g_mouse->dlt_y, ri->data.mouse.lLastY);

         if (!GetCursorPos(&crs_pos))
            WINRAW_SYS_WRN("GetCursorPos");
         else if (!ScreenToClient((HWND)video_driver_window_get(), &crs_pos))
            WINRAW_SYS_WRN("ScreenToClient");
         else
         {
            g_mouse->x = crs_pos.x;
            g_mouse->y = crs_pos.y;
         }
      }

      if (ri->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
         g_mouse->btn_l = true;
      else if (ri->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
         g_mouse->btn_l = false;

      if (ri->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
         g_mouse->btn_m = true;
      else if (ri->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
         g_mouse->btn_m = false;

      if (ri->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
         g_mouse->btn_r = true;
      else if (ri->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
         g_mouse->btn_r = false;

      if (ri->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
      {
         if ((SHORT)ri->data.mouse.usButtonData > 0)
            InterlockedExchange(&g_mouse->whl_u, 1);
         else if ((SHORT)ri->data.mouse.usButtonData < 0)
            InterlockedExchange(&g_mouse->whl_d, 1);
      }
   }

end:
   DefWindowProcA(wnd, msg, wpar, lpar);
   return 0;
}

static void *winraw_init(const char *joypad_driver)
{
   bool r             = false;
   winraw_input_t *wr = (winraw_input_t *)calloc(1, sizeof(winraw_input_t));
   g_keyboard         = (winraw_keyboard_t*)calloc(1, sizeof(winraw_keyboard_t));
   g_mouse            = (winraw_mouse_t*)calloc(1, sizeof(winraw_mouse_t));

   if (!wr || !g_keyboard || !g_mouse)
   {
      WINRAW_ERR("calloc failed.");
      goto error;
   }

   WINRAW_LOG("Initializing input driver ...");

   input_keymaps_init_keyboard_lut(rarch_key_map_winraw);

   wr->window = winraw_create_window(winraw_callback);
   if (!wr->window)
   {
      WINRAW_ERR("winraw_create_window failed.");
      goto error;
   }

   r = winraw_set_keyboard_input(wr->window);
   if (!r)
   {
      WINRAW_ERR("winraw_set_keyboard_input failed.");
      goto error;
   }

   r = winraw_set_mouse_input(wr->window, false);
   if (!r)
   {
      WINRAW_ERR("winraw_set_mouse_input failed.");
      goto error;
   }

   wr->joypad = input_joypad_init_driver(joypad_driver, wr);

   WINRAW_LOG("Input driver initialized.");
   return wr;

error:
   if (wr && wr->window)
   {
      winraw_set_mouse_input(NULL, false);
      winraw_set_keyboard_input(NULL);
      winraw_destroy_window(wr->window);
   }
   free(g_mouse);
   free(g_keyboard);
   free(wr);
   WINRAW_ERR("Input driver initialization failed.");
   return NULL;
}

static void winraw_poll(void *d)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   memcpy(&wr->keyboard, g_keyboard, sizeof(winraw_keyboard_t));

   wr->mouse.x = g_mouse->x;
   wr->mouse.y = g_mouse->y;
   wr->mouse.dlt_x = InterlockedExchange(&g_mouse->dlt_x, 0);
   wr->mouse.dlt_y = InterlockedExchange(&g_mouse->dlt_y, 0);
   wr->mouse.whl_u = InterlockedExchange(&g_mouse->whl_u, 0);
   wr->mouse.whl_d = InterlockedExchange(&g_mouse->whl_d, 0);
   wr->mouse.btn_l = g_mouse->btn_l;
   wr->mouse.btn_m = g_mouse->btn_m;
   wr->mouse.btn_r = g_mouse->btn_r;

   if (wr->joypad)
      wr->joypad->poll();
}

static int16_t winraw_input_state(void *d,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   switch (device)
   {
      case RETRO_DEVICE_KEYBOARD:
         return winraw_keyboard_state(wr, id);
      case RETRO_DEVICE_MOUSE:
         return winraw_mouse_state(wr, false, id);
      case RARCH_DEVICE_MOUSE_SCREEN:
         return winraw_mouse_state(wr, true, id);
      case RETRO_DEVICE_JOYPAD:
         return winraw_joypad_state(wr, joypad_info, binds[port], port, id);
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(wr->joypad, joypad_info,
                  port, index, id, binds[port]);
   }

   return 0;
}

static bool winraw_meta_key_pressed(void *u1, int u2)
{
   return false;
}

static void winraw_free(void *d)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   WINRAW_LOG("Deinitializing input driver ...");

   if (wr->joypad)
      wr->joypad->destroy();
   winraw_set_mouse_input(NULL, false);
   winraw_set_keyboard_input(NULL);
   winraw_destroy_window(wr->window);
   free(g_mouse);
   free(g_keyboard);
   free(wr);

   WINRAW_LOG("Input driver deinitialized.");
}

static uint64_t winraw_get_capabilities(void *u)
{
   return RETRO_DEVICE_KEYBOARD |
          RETRO_DEVICE_MOUSE |
          RETRO_DEVICE_JOYPAD |
          RETRO_DEVICE_ANALOG;
}

static void winraw_grab_mouse(void *d, bool grab)
{
   winraw_input_t *wr = (winraw_input_t*)d;
   bool r;

   if (grab == wr->mouse_grab)
      return;

   r = winraw_set_mouse_input(wr->window, grab);
   if (!r)
   {
      WINRAW_ERR("Mouse grab failed.");
      return;
   }

   wr->mouse_grab = grab;
}

static bool winraw_set_rumble(void *d, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   return input_joypad_set_rumble(wr->joypad, port, effect, strength);
}

static const input_device_driver_t *winraw_get_joypad_driver(void *d)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   return wr->joypad;
}

static bool winraw_keyboard_mapping_is_blocked(void *d)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   return wr->kbd_mapp_block;
}

static void winraw_keyboard_mapping_set_block(void *d, bool block)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   wr->kbd_mapp_block = block;
}

input_driver_t input_winraw = {
   winraw_init,
   winraw_poll,
   winraw_input_state,
   winraw_meta_key_pressed,
   winraw_free,
   NULL,
   NULL,
   winraw_get_capabilities,
   "raw",
   winraw_grab_mouse,
   NULL,
   winraw_set_rumble,
   winraw_get_joypad_driver,
   NULL,
   winraw_keyboard_mapping_is_blocked,
   winraw_keyboard_mapping_set_block,
};
