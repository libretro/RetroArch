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

#include <windows.h>

#ifdef CXX_BUILD
extern "C" {
#endif

#include <hidsdi.h>

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
#include <dbt.h>
#endif

#ifdef CXX_BUILD
}
#endif

#include <compat/strl.h>

#ifndef _XBOX
#include "../../gfx/common/win32_common.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../input_keymaps.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct
{
   uint8_t keys[SC_LAST];
   bool pause;
} winraw_keyboard_t;

typedef struct
{
   HANDLE hnd;
   LONG x, y, dlt_x, dlt_y;
   LONG whl_u, whl_d;
   bool btn_l, btn_m, btn_r, btn_b4, btn_b5;
   int device;
} winraw_mouse_t;

struct winraw_pointer_status
{
   struct winraw_pointer_status *next;
   int pointer_id;
   int pointer_x;
   int pointer_y;
};

typedef struct
{
   double view_abs_ratio_x;
   double view_abs_ratio_y;
   HWND window;
   /* Dummy head for easier iteration */
   struct winraw_pointer_status pointer_head; 
   RECT active_rect; /* Needed for checking for a windows size change */
   RECT prev_rect;   /* Needed for checking for a windows size change */
   int rect_delay;   /* Needed to delay resize of window */
   winraw_mouse_t *mice;
   unsigned mouse_cnt;
   winraw_keyboard_t keyboard;
   bool mouse_xy_mapping_ready;
   bool mouse_grab;
} winraw_input_t;

/* TODO/FIXME - static globals */
static winraw_mouse_t *g_mice        = NULL;
static bool winraw_focus             = false;

#define WINRAW_KEYBOARD_PRESSED(wr, key) (wr->keyboard.keys[rarch_keysym_lut[(enum retro_key)(key)]])

static HWND winraw_create_window(WNDPROC wnd_proc)
{
   HWND wnd;
   WNDCLASSA wc     = {0};

   wc.hInstance     = GetModuleHandleA(NULL);

   if (!wc.hInstance)
      return NULL;

   wc.lpfnWndProc   = wnd_proc;
   wc.lpszClassName = "winraw-input";
   if (     !RegisterClassA(&wc) 
         &&  GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      return NULL;

   if (!(wnd = CreateWindowExA(0, wc.lpszClassName,
               NULL, 0, 0, 0, 0, 0,
               HWND_MESSAGE, NULL, NULL, NULL)))
   {
      UnregisterClassA(wc.lpszClassName, NULL);
      return NULL;
   }

   return wnd;
}

static void winraw_destroy_window(HWND wnd)
{
   if (!wnd)
      return;

   DestroyWindow(wnd);
   UnregisterClassA("winraw-input", NULL);
}

static BOOL winraw_set_keyboard_input(HWND window)
{
   RAWINPUTDEVICE rid;
   settings_t *settings;

   settings        = config_get_ptr();

   rid.dwFlags     = window ? 0 : RIDEV_REMOVE;
   rid.hwndTarget  = window;
   rid.usUsagePage = 0x01; /* Generic desktop */
   rid.usUsage     = 0x06; /* Keyboard */
   if (settings->bools.input_nowinkey_enable)
      rid.dwFlags |= RIDEV_NOHOTKEYS; /* Disable win keys while focused */

   return RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
}

static void winraw_log_mice_info(winraw_mouse_t *mice, unsigned mouse_cnt)
{
   unsigned i;
   char name[256];
   UINT name_size = sizeof(name);
   char prod_name[128];
   wchar_t prod_buf[128];

   name[0] = '\0';

   for (i = 0; i < mouse_cnt; ++i)
   {
      UINT r = GetRawInputDeviceInfoA(mice[i].hnd, RIDI_DEVICENAME,
            name, &name_size);
      if (r == (UINT)-1 || r == 0)
         name[0] = '\0';

      prod_name[0] = '\0';
      prod_buf[0]  = '\0';
      if (name[0])
      {
         HANDLE hhid = CreateFile(name,
               0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

         if (hhid != INVALID_HANDLE_VALUE)
         {
            if (HidD_GetProductString (hhid, prod_buf, sizeof(prod_buf)))
               wcstombs(prod_name, prod_buf, sizeof(prod_name));
         }
         CloseHandle(hhid);
      }
      if (prod_name[0])
         strlcpy(name, prod_name, sizeof(name));

      if (!name[0])
         strlcpy(name, "<name not found>", sizeof(name));

      input_config_set_mouse_display_name(i, name);

      RARCH_LOG("[WinRaw]: Mouse #%u: \"%s\".\n", i, name);
   }
}

static bool winraw_init_devices(winraw_mouse_t **mice, unsigned *mouse_cnt)
{
   UINT i;
   POINT crs_pos;
   winraw_mouse_t *mice_r   = NULL;
   unsigned mouse_cnt_r     = 0;
   RAWINPUTDEVICELIST *devs = NULL;
   UINT dev_cnt             = 0;
   UINT r                   = GetRawInputDeviceList(
         NULL, &dev_cnt, sizeof(RAWINPUTDEVICELIST));

   if (r == (UINT)-1)
      goto error;

   devs = (RAWINPUTDEVICELIST*)malloc(
         dev_cnt * sizeof(RAWINPUTDEVICELIST));
   if (!devs)
      goto error;

   dev_cnt = GetRawInputDeviceList(devs,
         &dev_cnt, sizeof(RAWINPUTDEVICELIST));
   if (dev_cnt == (UINT)-1)
      goto error;

   for (i = 0; i < dev_cnt; ++i)
      mouse_cnt_r += devs[i].dwType == RIM_TYPEMOUSE ? 1 : 0;

   if (mouse_cnt_r)
   {
      mice_r = (winraw_mouse_t*)calloc(
            1, mouse_cnt_r * sizeof(winraw_mouse_t));
      if (!mice_r)
         goto error;

      if (!GetCursorPos(&crs_pos))
         goto error;

      for (i = 0; i < mouse_cnt_r; ++i)
      {
         mice_r[i].x = crs_pos.x;
         mice_r[i].y = crs_pos.y;
      }
   }

   /* count is already checked, so this is safe */
   for (i = mouse_cnt_r = 0; i < dev_cnt; ++i)
   {
      if (devs[i].dwType == RIM_TYPEMOUSE)
         mice_r[mouse_cnt_r++].hnd = devs[i].hDevice;
   }

   winraw_log_mice_info(mice_r, mouse_cnt_r);
   free(devs);

   *mice      = mice_r;
   *mouse_cnt = mouse_cnt_r;

   return true;

error:
   free(devs);
   free(mice_r);
   *mice      = NULL;
   *mouse_cnt = 0;
   return false;
}

static BOOL winraw_set_mouse_input(HWND window)
{
   RAWINPUTDEVICE rid;

   rid.dwFlags     = (window) ? 0 : RIDEV_REMOVE;
   rid.hwndTarget  = window;
   rid.usUsagePage = 0x01; /* generic desktop */
   rid.usUsage     = 0x02; /* mouse */

   return RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
}

static int16_t winraw_lightgun_aiming_state(winraw_input_t *wr,
      winraw_mouse_t *mouse,
      unsigned port, unsigned id)
{
   struct video_viewport vp;
   const int edge_detect = 32700;
   bool inside           = false;
   int16_t res_x         = 0;
   int16_t res_y         = 0;
   int16_t res_screen_x  = 0;
   int16_t res_screen_y  = 0;

   vp.x                  = 0;
   vp.y                  = 0;
   vp.width              = 0;
   vp.height             = 0;
   vp.full_width         = 0;
   vp.full_height        = 0;

   if (!(video_driver_translate_coord_viewport_wrap(
               &vp, mouse->x, mouse->y,
               &res_x, &res_y, &res_screen_x, &res_screen_y)))
      return 0;

   inside =    (res_x >= -edge_detect) 
            && (res_y >= -edge_detect)
            && (res_x <= edge_detect)
            && (res_y <= edge_detect);

   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
         if (inside)
            return res_x;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
         if (inside)
            return res_y;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
         return !inside;
      default:
         break;
   }

   return 0;
}

static bool winraw_mouse_button_pressed(
      winraw_input_t *wr,
      winraw_mouse_t *mouse,
      unsigned port, unsigned key)
{
	switch (key)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return mouse->btn_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return mouse->btn_r;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return mouse->btn_m;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return mouse->btn_b4;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return mouse->btn_b5;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return mouse->whl_u;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return mouse->whl_d;
   }

	return false;
}

static void winraw_init_mouse_xy_mapping(winraw_input_t *wr)
{
   struct video_viewport viewport;

   if (video_driver_get_viewport_info(&viewport))
   {
      unsigned i;
      int center_x               = viewport.x + viewport.width / 2;
      int center_y               = viewport.y + viewport.height / 2;

      for (i = 0; i < wr->mouse_cnt; ++i)
      {
         g_mice[i].x             = center_x;
         g_mice[i].y             = center_y;
      }

      wr->view_abs_ratio_x       = (double)viewport.full_width / 65535.0;
      wr->view_abs_ratio_y       = (double)viewport.full_height / 65535.0;

      wr->mouse_xy_mapping_ready = true;
   }
}

static void winraw_update_mouse_state(winraw_input_t *wr, 
      winraw_mouse_t *mouse, RAWMOUSE *state)
{
   POINT crs_pos;

   /* Used for fixing coordinates after switching resolutions */
   GetClientRect((HWND)video_driver_window_get(), &wr->prev_rect);

   if (!EqualRect(&wr->active_rect, &wr->prev_rect))
   {
      if (wr->rect_delay < 10)
      {
          winraw_init_mouse_xy_mapping(wr); /* Triggering fewer times seems to fix the issue. Forcing resize while resolution is changing */
          wr->rect_delay ++;
      }
      else
      {
	      int bottom      = wr->prev_rect.bottom;
	      int right       = wr->prev_rect.right;
	      wr->active_rect = wr->prev_rect;
	      winraw_init_mouse_xy_mapping(wr);
	      wr->rect_delay  = 0;
      }
   }

   if (state->usFlags & MOUSE_MOVE_ABSOLUTE)
   {
      if (wr->mouse_xy_mapping_ready)
      {
         state->lLastX = (LONG)(wr->view_abs_ratio_x * state->lLastX);
         state->lLastY = (LONG)(wr->view_abs_ratio_y * state->lLastY);
         InterlockedExchangeAdd(&mouse->dlt_x, state->lLastX - mouse->x);
         InterlockedExchangeAdd(&mouse->dlt_y, state->lLastY - mouse->y);
         mouse->x      = state->lLastX;
         mouse->y      = state->lLastY;
      }
      else
         winraw_init_mouse_xy_mapping(wr);
   }
   else if (state->lLastX || state->lLastY)
   {
      /* Menu and pointer require GetCursorPos() for
       * positioning, but using that always will
       * break multiple mice positions */
      bool getcursorpos = (mouse->device == RETRO_DEVICE_POINTER) ? true : false;
#ifdef HAVE_MENU
      if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
         getcursorpos = true;
#endif

      if (getcursorpos)
      {
         if (!GetCursorPos(&crs_pos))
            RARCH_DBG("[WinRaw]: GetCursorPos failed with error %lu.\n", GetLastError());
         else if (!ScreenToClient((HWND)video_driver_window_get(), &crs_pos))
            RARCH_DBG("[WinRaw]: ScreenToClient failed with error %lu.\n", GetLastError());
      }
      else
      {
         /* Handle different sensitivity for lightguns */
         if (mouse->device == RETRO_DEVICE_LIGHTGUN)
         {
            InterlockedExchange(&mouse->dlt_x, state->lLastX);
            InterlockedExchange(&mouse->dlt_y, state->lLastY);
         }
         else
         {
            InterlockedExchangeAdd(&mouse->dlt_x, state->lLastX);
            InterlockedExchangeAdd(&mouse->dlt_y, state->lLastY);
         }

         crs_pos.x = mouse->x + mouse->dlt_x;
         crs_pos.y = mouse->y + mouse->dlt_y;

         /* Prevent travel outside active window */
         if (crs_pos.x < wr->active_rect.left)
            crs_pos.x = wr->active_rect.left;
         else if (crs_pos.x > wr->active_rect.right)
            crs_pos.x = wr->active_rect.right;

         if (crs_pos.y < wr->active_rect.top)
            crs_pos.y = wr->active_rect.top;
         else if (crs_pos.y > wr->active_rect.bottom)
            crs_pos.y = wr->active_rect.bottom;
      }

      mouse->x = crs_pos.x;
      mouse->y = crs_pos.y;
   }

   if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
      mouse->btn_l = true;
   else if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
      mouse->btn_l = false;

   if (state->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
      mouse->btn_m  = true;
   else if (state->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
      mouse->btn_m  = false;

   if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
      mouse->btn_r  = true;
   else if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
      mouse->btn_r  = false;

   if (state->usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
      mouse->btn_b4 = true;
   else if (state->usButtonFlags & RI_MOUSE_BUTTON_4_UP)
      mouse->btn_b4 = false;

   if (state->usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
      mouse->btn_b5 = true;
   else if (state->usButtonFlags & RI_MOUSE_BUTTON_5_UP)
      mouse->btn_b5 = false;

   if (state->usButtonFlags & RI_MOUSE_WHEEL)
   {
      if ((SHORT)state->usButtonData > 0)
         InterlockedExchange(&mouse->whl_u, 1);
      else if ((SHORT)state->usButtonData < 0)
         InterlockedExchange(&mouse->whl_d, 1);
   }
}

static LRESULT CALLBACK winraw_callback(
      HWND wnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
   unsigned i;
   unsigned mcode, flags, kdown;
   static uint8_t data[1024];
   RAWINPUT       *ri = (RAWINPUT*)data;
   UINT size          = sizeof(data);
   winraw_input_t *wr = (winraw_input_t*)(LONG_PTR)
      GetWindowLongPtr(wnd, GWLP_USERDATA);

   if (msg != WM_INPUT)
      return DefWindowProcA(wnd, msg, wpar, lpar);

   if (
          GET_RAWINPUT_CODE_WPARAM(wpar) != RIM_INPUT  /* app is in the background */
       || GetRawInputData((HRAWINPUT)lpar, RID_INPUT,
         data, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
   {
      DefWindowProcA(wnd, msg, wpar, lpar);
      return 0;
   }

   switch (ri->header.dwType)
   {
      case RIM_TYPEKEYBOARD:
         mcode = ri->data.keyboard.MakeCode;
         flags = ri->data.keyboard.Flags;
         kdown = (flags & RI_KEY_BREAK) ? 0 : 1;

         /* Extended scancodes */
         if (flags & RI_KEY_E0)
            mcode |= 0xE000;
         else if (flags & RI_KEY_E1)
            mcode |= 0xE100;

         /* Special pause-key handling due to
          * scancode 0xE11D45 incoming separately */
         if (wr->keyboard.pause)
         {
            wr->keyboard.pause = false;
            if (mcode == SC_NUMLOCK)
               mcode = SC_PAUSE;
         }
         else if (mcode == 0xE11D)
            wr->keyboard.pause = true;

         /* Ignored scancodes */
         switch (mcode)
         {
            case RETROK_UNKNOWN:
            case 0xE11D:
            case 0xE02A:
            case 0xE036:
            case 0xE0AA:
            case 0xE0B6:
               return 0;
         }

         wr->keyboard.keys[mcode] = kdown;
         input_keyboard_event(kdown,
               input_keymaps_translate_keysym_to_rk(mcode),
               0, 0, RETRO_DEVICE_KEYBOARD);
         break;
      case RIM_TYPEMOUSE:
         for (i = 0; i < wr->mouse_cnt; ++i)
         {
            if (g_mice[i].hnd == ri->header.hDevice)
            {
               winraw_update_mouse_state(wr,
                     &g_mice[i], &ri->data.mouse);
               break;
            }
         }
         break;
   }

   DefWindowProcA(wnd, msg, wpar, lpar);
   return 0;
}

static void *winraw_init(const char *joypad_driver)
{
   winraw_input_t *wr = (winraw_input_t *)
      calloc(1, sizeof(winraw_input_t));

   if (!wr)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_winraw);

   wr->window = winraw_create_window(winraw_callback);
   if (!wr->window)
      goto error;

   if (!winraw_init_devices(&g_mice, &wr->mouse_cnt))
      goto error;

   if (wr->mouse_cnt)
   {
      wr->mice = (winraw_mouse_t*)
         malloc(wr->mouse_cnt * sizeof(winraw_mouse_t));
      if (!wr->mice)
         goto error;

      memcpy(wr->mice, g_mice, wr->mouse_cnt * sizeof(winraw_mouse_t));
   }

   if (!winraw_set_keyboard_input(wr->window))
      goto error;

   if (!winraw_set_mouse_input(wr->window))
      goto error;

   SetWindowLongPtr(wr->window, GWLP_USERDATA, (LONG_PTR)wr);

   return wr;

error:
   if (wr && wr->window)
   {
      winraw_set_mouse_input(NULL);
      winraw_set_keyboard_input(NULL);
      winraw_destroy_window(wr->window);
   }
   free(g_mice);
   if (wr)
      free(wr->mice);
   free(wr);
   return NULL;
}

static void winraw_poll(void *data)
{
   unsigned i;
   winraw_input_t *wr     = (winraw_input_t*)data;

   for (i = 0; i < wr->mouse_cnt; ++i)
   {
      wr->mice[i].x       = g_mice[i].x;
      wr->mice[i].y       = g_mice[i].y;
      wr->mice[i].dlt_x   = InterlockedExchange(&g_mice[i].dlt_x, 0);
      wr->mice[i].dlt_y   = InterlockedExchange(&g_mice[i].dlt_y, 0);
      wr->mice[i].whl_u   = InterlockedExchange(&g_mice[i].whl_u, 0);
      wr->mice[i].whl_d   = InterlockedExchange(&g_mice[i].whl_d, 0);
      wr->mice[i].btn_l   = g_mice[i].btn_l;
      wr->mice[i].btn_m   = g_mice[i].btn_m;
      wr->mice[i].btn_r   = g_mice[i].btn_r;
      wr->mice[i].btn_b4  = g_mice[i].btn_b4;
      wr->mice[i].btn_b5  = g_mice[i].btn_b5;
   }

   /* Prevent LAlt sticky after unfocusing with Alt-Tab */
   if (     !winraw_focus
         && wr->keyboard.keys[SC_LALT] 
         && !(GetKeyState(VK_MENU) & 0x8000))
   {
      wr->keyboard.keys[SC_LALT] = 0;
      input_keyboard_event(0,
            input_keymaps_translate_keysym_to_rk(SC_LALT),
            0, 0, RETRO_DEVICE_KEYBOARD);
   }
   /* Clear all keyboard key states when unfocused */
   else if (!winraw_focus && !(GetKeyState(VK_MENU) & 0x8000))
      memset(wr->keyboard.keys, 0, SC_LAST);
}

static unsigned winraw_retro_id_to_rarch(unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
         return RARCH_LIGHTGUN_DPAD_RIGHT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
         return RARCH_LIGHTGUN_DPAD_LEFT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
         return RARCH_LIGHTGUN_DPAD_UP;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
         return RARCH_LIGHTGUN_DPAD_DOWN;
      case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
         return RARCH_LIGHTGUN_SELECT;
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return RARCH_LIGHTGUN_START;
      case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
         return RARCH_LIGHTGUN_RELOAD;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return RARCH_LIGHTGUN_TRIGGER;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
         return RARCH_LIGHTGUN_AUX_A;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
         return RARCH_LIGHTGUN_AUX_B;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
         return RARCH_LIGHTGUN_AUX_C;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return RARCH_LIGHTGUN_START;
      default:
         break;
   }

   return 0;
}

static int16_t winraw_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   int16_t ret           = 0;
   settings_t *settings  = NULL;
   winraw_mouse_t *mouse = NULL;
   winraw_input_t *wr    = (winraw_input_t*)data;
   bool process_mouse    = 
         (device == RETRO_DEVICE_JOYPAD)
      || (device == RETRO_DEVICE_MOUSE)
      || (device == RARCH_DEVICE_MOUSE_SCREEN)
      || (device == RETRO_DEVICE_LIGHTGUN)
      || (device == RETRO_DEVICE_POINTER)
      || (device == RARCH_DEVICE_POINTER_SCREEN);

   if (port >= MAX_USERS)
      return 0;

   if (process_mouse)
   {
      unsigned i;
      settings        = config_get_ptr();
      for (i = 0; i < wr->mouse_cnt; ++i)
      {
         if (i == settings->uints.input_mouse_index[port])
         {
            mouse = &wr->mice[i];
            if (mouse && device > RETRO_DEVICE_JOYPAD)
               g_mice[i].device = device;
            break;
         }
      }
   }

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;

            if (mouse)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (winraw_mouse_button_pressed(wr,
                              mouse, port, binds[port][i].mbutton))
                        ret |= (1 << i);
                  }
               }
            }

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if ((binds[port][i].key < RETROK_LAST) && 
                        WINRAW_KEYBOARD_PRESSED(wr, binds[port][i].key))
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if (
                     (binds[port][id].key < RETROK_LAST) 
                     && WINRAW_KEYBOARD_PRESSED(wr, binds[port][id].key)
                     && ((    id == RARCH_GAME_FOCUS_TOGGLE) 
                        || !keyboard_mapping_blocked)
                     )
                  return 1;
               else if (mouse && winraw_mouse_button_pressed(wr,
                        mouse, port, binds[port][id].mbutton))
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key < RETROK_LAST)
            {
               if (WINRAW_KEYBOARD_PRESSED(wr, id_plus_key))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key < RETROK_LAST)
            {
               if (WINRAW_KEYBOARD_PRESSED(wr, id_minus_key))
                  ret += -0x7fff;
            }
         }
         return ret;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && WINRAW_KEYBOARD_PRESSED(wr, id);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         if (mouse)
         {
            bool abs = (device == RARCH_DEVICE_MOUSE_SCREEN);
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_X:
                  return abs ? mouse->x : mouse->dlt_x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return abs ? mouse->y : mouse->dlt_y;
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  if (mouse->btn_l)
                     return 1;
                  break;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  if (mouse->btn_r)
                     return 1;
                  break;
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  if (mouse->whl_u)
                     return 1;
                  break;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  if (mouse->whl_d)
                     return 1;
                  break;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  if (mouse->btn_m)
                     return 1;
                  break;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
                  if (mouse->btn_b4)
                     return 1;
                  break;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
                  if (mouse->btn_b5)
                     return 1;
                  break;
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            struct video_viewport vp;
            bool pointer_down           = false;
            bool inside                 = false;
            int x                       = 0;
            int y                       = 0;
            int16_t res_x               = 0;
            int16_t res_y               = 0;
            int16_t res_screen_x        = 0;
            int16_t res_screen_y        = 0;
            unsigned num                = 0;
            struct winraw_pointer_status *
               check_pos                = wr->pointer_head.next;

            vp.x                        = 0;
            vp.y                        = 0;
            vp.width                    = 0;
            vp.height                   = 0;
            vp.full_width               = 0;
            vp.full_height              = 0;

            while (check_pos && num < idx)
            {
               num++;
               check_pos    = check_pos->next;
            }
            if (!check_pos && idx > 0) /* idx = 0 has mouse fallback. */
               return 0;

            if (mouse)
            {
               x            = mouse->x;
               y            = mouse->y;
               pointer_down = mouse->btn_l;
            }

            if (check_pos)
            {
               x            = check_pos->pointer_x;
               y            = check_pos->pointer_y;
               pointer_down = true;
            }

            if (!(video_driver_translate_coord_viewport_wrap(&vp, x, y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y)))
               return 0;

            if (device == RARCH_DEVICE_POINTER_SCREEN)
            {
               res_x        = res_screen_x;
               res_y        = res_screen_y;
            }

            if (!(inside = (res_x >= -0x7fff) && (res_y >= -0x7fff)))
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
         }
         break;
      case RETRO_DEVICE_LIGHTGUN:
			switch (id)
			{
				/*aiming*/
				case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
				case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
				case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
					if (mouse)
						return winraw_lightgun_aiming_state(wr, mouse, port, id);
					break;
				/*buttons*/
				case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
				case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
				case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
				case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
				case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
				case RETRO_DEVICE_ID_LIGHTGUN_START:
				case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
				case RETRO_DEVICE_ID_LIGHTGUN_PAUSE: /* deprecated */
               {
                  unsigned new_id                = winraw_retro_id_to_rarch(id);
                  const uint64_t bind_joykey     = input_config_binds[port][new_id].joykey;
                  const uint64_t bind_joyaxis    = input_config_binds[port][new_id].joyaxis;
                  const uint64_t autobind_joykey = input_autoconf_binds[port][new_id].joykey;
                  const uint64_t autobind_joyaxis= input_autoconf_binds[port][new_id].joyaxis;
                  uint16_t port                  = joypad_info->joy_idx;
                  float axis_threshold           = joypad_info->axis_threshold;
                  const uint64_t joykey          = (bind_joykey != NO_BTN)
                     ? bind_joykey  : autobind_joykey;
                  const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
                     ? bind_joyaxis : autobind_joyaxis;
                  if (binds[port][new_id].valid)
                  {
                     if ((uint16_t)joykey != NO_BTN && joypad->button(
                              port, (uint16_t)joykey))
                        return 1;
                     if (joyaxis != AXIS_NONE &&
                           ((float)abs(joypad->axis(port, joyaxis)) 
                            / 0x8000) > axis_threshold)
                        return 1;
                     else if (
                           binds[port][new_id].key < RETROK_LAST
                           && !keyboard_mapping_blocked
                           && WINRAW_KEYBOARD_PRESSED(wr, binds[port]
                              [new_id].key)
                           )
                        return 1;
                     else
                     {
                        if (
                              mouse && winraw_mouse_button_pressed(wr,
                                 mouse, port, binds[port][new_id].mbutton)
                           )
                           return 1;
                     }
                  }
               }
               break;
               /*deprecated*/
            case RETRO_DEVICE_ID_LIGHTGUN_X:
               if (mouse)
                  return mouse->dlt_x;
               break;
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               if (mouse)
                  return mouse->dlt_y;
               break;
         }
         break;
   }

   return 0;
}

#if !defined(_XBOX)
bool winraw_handle_message(UINT msg,
      WPARAM wpar, LPARAM lpar)
{
   switch (msg)
   {
      case WM_SETFOCUS:
         winraw_focus = true;
         break;
      case WM_KILLFOCUS:
         winraw_focus = false;
         break;

      case WM_DEVICECHANGE:
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
         if (wpar == DBT_DEVICEARRIVAL ||
             wpar == DBT_DEVICEREMOVECOMPLETE)
         {
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lpar;
            if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
               joypad_driver_reinit(NULL, NULL);
         }
#endif
         break;
   }
   return false;
}
#endif

static void winraw_free(void *data)
{
   winraw_input_t *wr = (winraw_input_t*)data;

   winraw_set_mouse_input(NULL);
   winraw_set_keyboard_input(NULL);
   SetWindowLongPtr(wr->window, GWLP_USERDATA, 0);
   winraw_destroy_window(wr->window);
   free(g_mice);
   free(wr->mice);

   free(data);
}

static uint64_t winraw_get_capabilities(void *u)
{
   return   (1 << RETRO_DEVICE_KEYBOARD)
          | (1 << RETRO_DEVICE_MOUSE)
          | (1 << RETRO_DEVICE_JOYPAD)
          | (1 << RETRO_DEVICE_ANALOG)
          | (1 << RETRO_DEVICE_POINTER)
          | (1 << RETRO_DEVICE_LIGHTGUN);
}

static void winraw_grab_mouse(void *d, bool state)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   if (state == wr->mouse_grab)
      return;

   if (!winraw_set_mouse_input(wr->window))
      return;

   wr->mouse_grab = state;

#ifndef _XBOX
   win32_clip_window(state);
#endif
}

input_driver_t input_winraw = {
   winraw_init,
   winraw_poll,
   winraw_input_state,
   winraw_free,
   NULL,
   NULL,
   winraw_get_capabilities,
   "raw",
   winraw_grab_mouse,
   NULL,
   NULL
};
