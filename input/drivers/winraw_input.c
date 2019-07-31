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

#include "../input_keymaps.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct
{
   uint8_t keys[256];
} winraw_keyboard_t;

typedef struct
{
   HANDLE hnd;
   LONG x, y, dlt_x, dlt_y;
   LONG whl_u, whl_d;
   bool btn_l, btn_m, btn_r, btn_b4, btn_b5;
} winraw_mouse_t;

typedef struct
{
   bool mouse_grab;
   winraw_keyboard_t keyboard;
   HWND window;
   winraw_mouse_t *mice;
   const input_device_driver_t *joypad;
} winraw_input_t;

static winraw_keyboard_t *g_keyboard = NULL;
static winraw_mouse_t *g_mice        = NULL;
static unsigned g_mouse_cnt          = 0;
static bool g_mouse_xy_mapping_ready = false;
static double g_view_abs_ratio_x     = 0.0;
static double g_view_abs_ratio_y     = 0.0;

static HWND winraw_create_window(WNDPROC wnd_proc)
{
   HWND wnd;
   WNDCLASSA wc = {0};

   wc.hInstance = GetModuleHandleA(NULL);

   if (!wc.hInstance)
   {
      RARCH_ERR("[WINRAW]: GetModuleHandleA failed with error %lu.\n", GetLastError());
      return NULL;
   }

   wc.lpfnWndProc   = wnd_proc;
   wc.lpszClassName = "winraw-input";
   if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
   {
      RARCH_ERR("[WINRAW]: RegisterClassA failed with error %lu.\n", GetLastError());
      return NULL;
   }

   wnd = CreateWindowExA(0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0,
         HWND_MESSAGE, NULL, NULL, NULL);
   if (!wnd)
   {
      RARCH_ERR("[WINRAW]: CreateWindowExA failed with error %lu.\n", GetLastError());
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
   {
      RARCH_WARN("[WINRAW]: DestroyWindow failed with error %lu.\n", GetLastError());
   }

   r = UnregisterClassA("winraw-input", NULL);

   if (!r)
   {
      RARCH_WARN("[WINRAW]: UnregisterClassA failed with error %lu.\n", GetLastError());
   }
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
      RARCH_ERR("[WINRAW]: RegisterRawInputDevices failed with error %lu.\n", GetLastError());
      return false;
   }

   return true;
}

static void winraw_log_mice_info(winraw_mouse_t *mice, unsigned mouse_cnt)
{
   unsigned i;
   char name[256];
   UINT name_size = sizeof(name);

   name[0] = '\0';

   for (i = 0; i < mouse_cnt; ++i)
   {
      UINT r = GetRawInputDeviceInfoA(mice[i].hnd, RIDI_DEVICENAME,
            name, &name_size);
      if (r == (UINT)-1 || r == 0)
         name[0] = '\0';
      RARCH_LOG("[WINRAW]: Mouse #%u %s.\n", i, name);
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
   {
      RARCH_ERR("[WINRAW]: GetRawInputDeviceList failed with error %lu.\n", GetLastError());
      goto error;
   }

   devs = (RAWINPUTDEVICELIST*)malloc(dev_cnt * sizeof(RAWINPUTDEVICELIST));
   if (!devs)
      goto error;

   dev_cnt = GetRawInputDeviceList(devs, &dev_cnt, sizeof(RAWINPUTDEVICELIST));
   if (dev_cnt == (UINT)-1)
   {
      RARCH_ERR("[WINRAW]: GetRawInputDeviceList failed with error %lu.\n", GetLastError());
      goto error;
   }

   for (i = 0; i < dev_cnt; ++i)
      mouse_cnt_r += devs[i].dwType == RIM_TYPEMOUSE ? 1 : 0;

   if (mouse_cnt_r)
   {
      mice_r = (winraw_mouse_t*)calloc(1, mouse_cnt_r * sizeof(winraw_mouse_t));
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
   *mice = NULL;
   *mouse_cnt = 0;
   return false;
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
      RARCH_ERR("[WINRAW]: RegisterRawInputDevice failed with error %lu.\n", GetLastError());
      return false;
   }

   return true;
}

static int16_t winraw_lightgun_aiming_state(winraw_input_t *wr,
      unsigned port, unsigned id)
{
   const int edge_detect = 32700;
   struct video_viewport vp;
   bool inside = false;
   unsigned i;
   settings_t *settings  = config_get_ptr();
   winraw_mouse_t *mouse = NULL;
   int16_t res_x = 0;
   int16_t res_y = 0;
   int16_t res_screen_x = 0;
   int16_t res_screen_y = 0;

   if (port >= MAX_USERS)
      return 0;

   for (i = 0; i < g_mouse_cnt; ++i)
   {
      if (i == settings->uints.input_mouse_index[port])
      {
         mouse = &wr->mice[i];
         break;
      }
   }

   if (!mouse)
      return 0;

   vp.x = 0;
   vp.y = 0;
   vp.width = 0;
   vp.height = 0;
   vp.full_width = 0;
   vp.full_height = 0;

   if ( !( video_driver_translate_coord_viewport_wrap(
               &vp, mouse->x, mouse->y, &res_x, &res_y, &res_screen_x, &res_screen_y ) ) )
      return 0;

   inside = (res_x >= -edge_detect) && (res_y >= -edge_detect) && (res_x <= edge_detect) && (res_y <= edge_detect);

   switch ( id )
   {
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
         return inside ? res_x : 0;
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
         return inside ? res_y : 0;
      case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
         return !inside;
      default:
         break;
   }

   return 0;
}

static int16_t winraw_mouse_state(winraw_input_t *wr,
      unsigned port, bool abs, unsigned id)
{
   unsigned i;
   settings_t *settings  = config_get_ptr();
   winraw_mouse_t *mouse = NULL;

   if (port >= MAX_USERS)
      return 0;

   for (i = 0; i < g_mouse_cnt; ++i)
   {
      if (i == settings->uints.input_mouse_index[port])
      {
         mouse = &wr->mice[i];
         break;
      }
   }

   if (!mouse)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return abs ? mouse->x : mouse->dlt_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return abs ? mouse->y : mouse->dlt_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return mouse->btn_l ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return mouse->btn_r ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return mouse->whl_u ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return mouse->whl_d ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return mouse->btn_m ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return mouse->btn_b4 ? 1 : 0;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return mouse->btn_b5 ? 1 : 0;
   }

   return 0;
}

#define winraw_keyboard_pressed(wr, key) (wr->keyboard.keys[rarch_keysym_lut[(enum retro_key)(key)]])

static bool winraw_mouse_button_pressed(
      winraw_input_t *wr, unsigned port, unsigned key)
{
	unsigned i;
	winraw_mouse_t *mouse = NULL;
	settings_t *settings  = config_get_ptr();

	if (port >= MAX_USERS)
		return false;

	for (i = 0; i < g_mouse_cnt; ++i)
	{
		if (i == settings->uints.input_mouse_index[port])
		{
			mouse = &wr->mice[i];
			break;
		}
	}

	if (!mouse)
		return false;

	switch ( key )
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

static bool winraw_is_pressed(winraw_input_t *wr,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind *binds,
      unsigned port, unsigned id)
{
   const struct retro_keybind *bind = &binds[id];

   if ((bind->key < RETROK_LAST) && winraw_keyboard_pressed(wr, bind->key))
      if ((id == RARCH_GAME_FOCUS_TOGGLE) || !input_winraw.keyboard_mapping_blocked)
         return true;
   if (binds && binds[id].valid)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[id].joykey != NO_BTN)
         ? binds[id].joykey : joypad_info.auto_binds[id].joykey;
      const uint32_t joyaxis = (binds[id].joyaxis != AXIS_NONE)
         ? binds[id].joyaxis : joypad_info.auto_binds[id].joyaxis;
      if (winraw_mouse_button_pressed(wr, port, bind->mbutton))
         return true;
      if ((uint16_t)joykey != NO_BTN && 
            wr->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
         return true;
      if (((float)abs(wr->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
         return true;
   }

   return false;
}

static void winraw_init_mouse_xy_mapping(void)
{
   struct video_viewport viewport;
   int center_x;
   int center_y;
   unsigned i;

   if (video_driver_get_viewport_info(&viewport))
   {
      center_x = viewport.x + viewport.width / 2;
      center_y = viewport.y + viewport.height / 2;

      for (i = 0; i < g_mouse_cnt; ++i)
      {
         g_mice[i].x = center_x;
         g_mice[i].y = center_y;
      }

      g_view_abs_ratio_x = (double)viewport.full_width / 65535.0;
      g_view_abs_ratio_y = (double)viewport.full_height / 65535.0;

      g_mouse_xy_mapping_ready = true;
   }
}

static int16_t winraw_deprecated_lightgun_state(winraw_input_t *wr,
      unsigned port, unsigned id)
{
   unsigned i;
   settings_t *settings  = config_get_ptr();
   winraw_mouse_t *mouse = NULL;

   if (port >= MAX_USERS)
      return 0;

   for (i = 0; i < g_mouse_cnt; ++i)
   {
      if (i == settings->uints.input_mouse_index[port])
      {
         mouse = &wr->mice[i];
         break;
      }
   }

   if (!mouse)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return mouse->dlt_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return mouse->dlt_y;
   }

   return 0;
}

static void winraw_update_mouse_state(winraw_mouse_t *mouse, RAWMOUSE *state)
{
   POINT crs_pos;

   if (state->usFlags & MOUSE_MOVE_ABSOLUTE)
   {
      if (g_mouse_xy_mapping_ready)
      {
         state->lLastX = (LONG)(g_view_abs_ratio_x * state->lLastX);
         state->lLastY = (LONG)(g_view_abs_ratio_y * state->lLastY);
         InterlockedExchangeAdd(&mouse->dlt_x, state->lLastX - mouse->x);
         InterlockedExchangeAdd(&mouse->dlt_y, state->lLastY - mouse->y);
         mouse->x = state->lLastX;
         mouse->y = state->lLastY;
      }
      else
         winraw_init_mouse_xy_mapping();
   }
   else if (state->lLastX || state->lLastY)
   {
      InterlockedExchangeAdd(&mouse->dlt_x, state->lLastX);
      InterlockedExchangeAdd(&mouse->dlt_y, state->lLastY);

      if (!GetCursorPos(&crs_pos))
      {
         RARCH_WARN("[WINRAW]: GetCursorPos failed with error %lu.\n", GetLastError());
      }
      else if (!ScreenToClient((HWND)video_driver_window_get(), &crs_pos))
      {
         RARCH_WARN("[WINRAW]: ScreenToClient failed with error %lu.\n", GetLastError());
      }
      else
      {
         mouse->x = crs_pos.x;
         mouse->y = crs_pos.y;
      }
   }

   if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
      mouse->btn_l = true;
   else if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
      mouse->btn_l = false;

   if (state->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
      mouse->btn_m = true;
   else if (state->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
      mouse->btn_m = false;

   if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
      mouse->btn_r = true;
   else if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
      mouse->btn_r = false;

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

static LRESULT CALLBACK winraw_callback(HWND wnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
   static uint8_t data[1024];
   UINT r;
   unsigned i;
   RAWINPUT *ri = (RAWINPUT*)data;
   UINT size    = sizeof(data);

   if (msg != WM_INPUT)
      return DefWindowProcA(wnd, msg, wpar, lpar);

   /* app is in the background */
   if (GET_RAWINPUT_CODE_WPARAM(wpar) != RIM_INPUT)
      goto end;

   r = GetRawInputData((HRAWINPUT)lpar, RID_INPUT,
         data, &size, sizeof(RAWINPUTHEADER));
   if (r == (UINT)-1)
   {
      RARCH_WARN("[WINRAW]: GetRawInputData failed with error %lu.\n",
            GetLastError());
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
      for (i = 0; i < g_mouse_cnt; ++i)
      {
         if (g_mice[i].hnd == ri->header.hDevice)
         {
            winraw_update_mouse_state(&g_mice[i], &ri->data.mouse);
            break;
         }
      }
   }

end:
   DefWindowProcA(wnd, msg, wpar, lpar);
   return 0;
}

static void *winraw_init(const char *joypad_driver)
{
   bool r;
   winraw_input_t *wr = (winraw_input_t *)
      calloc(1, sizeof(winraw_input_t));
   g_keyboard         = (winraw_keyboard_t*)
      calloc(1, sizeof(winraw_keyboard_t));

   if (!wr || !g_keyboard)
      goto error;

   RARCH_LOG("[WINRAW]: Initializing input driver... \n");

   input_keymaps_init_keyboard_lut(rarch_key_map_winraw);

   wr->window = winraw_create_window(winraw_callback);
   if (!wr->window)
      goto error;

   r = winraw_init_devices(&g_mice, &g_mouse_cnt);
   if (!r)
      goto error;

   if (!g_mouse_cnt)
   {
      RARCH_LOG("[WINRAW]: Mouse unavailable.\n");
   }
   else
   {
      wr->mice = (winraw_mouse_t*)
         malloc(g_mouse_cnt * sizeof(winraw_mouse_t));
      if (!wr->mice)
         goto error;

      memcpy(wr->mice, g_mice, g_mouse_cnt * sizeof(winraw_mouse_t));
   }

   r = winraw_set_keyboard_input(wr->window);
   if (!r)
      goto error;

   r = winraw_set_mouse_input(wr->window, false);
   if (!r)
      goto error;

   wr->joypad = input_joypad_init_driver(joypad_driver, wr);

   return wr;

error:
   if (wr && wr->window)
   {
      winraw_set_mouse_input(NULL, false);
      winraw_set_keyboard_input(NULL);
      winraw_destroy_window(wr->window);
   }
   free(g_keyboard);
   free(g_mice);
   if (wr)
      free(wr->mice);
   free(wr);
   return NULL;
}

static void winraw_poll(void *d)
{
   unsigned i;
   winraw_input_t *wr = (winraw_input_t*)d;

   memcpy(&wr->keyboard, g_keyboard, sizeof(winraw_keyboard_t));

   /* following keys are not handled by windows raw input api */
   wr->keyboard.keys[VK_LCONTROL] = GetAsyncKeyState(VK_LCONTROL) >> 1 ? 1 : 0;
   wr->keyboard.keys[VK_RCONTROL] = GetAsyncKeyState(VK_RCONTROL) >> 1 ? 1 : 0;
   wr->keyboard.keys[VK_LMENU]    = GetAsyncKeyState(VK_LMENU)    >> 1 ? 1 : 0;
   wr->keyboard.keys[VK_RMENU]    = GetAsyncKeyState(VK_RMENU)    >> 1 ? 1 : 0;
   wr->keyboard.keys[VK_LSHIFT]   = GetAsyncKeyState(VK_LSHIFT)   >> 1 ? 1 : 0;
   wr->keyboard.keys[VK_RSHIFT]   = GetAsyncKeyState(VK_RSHIFT)   >> 1 ? 1 : 0;

   for (i = 0; i < g_mouse_cnt; ++i)
   {
      wr->mice[i].x     = g_mice[i].x;
      wr->mice[i].y     = g_mice[i].y;
      wr->mice[i].dlt_x = InterlockedExchange(&g_mice[i].dlt_x, 0);
      wr->mice[i].dlt_y = InterlockedExchange(&g_mice[i].dlt_y, 0);
      wr->mice[i].whl_u = InterlockedExchange(&g_mice[i].whl_u, 0);
      wr->mice[i].whl_d = InterlockedExchange(&g_mice[i].whl_d, 0);
      wr->mice[i].btn_l = g_mice[i].btn_l;
      wr->mice[i].btn_m = g_mice[i].btn_m;
      wr->mice[i].btn_r = g_mice[i].btn_r;
      wr->mice[i].btn_b4 = g_mice[i].btn_b4;
      wr->mice[i].btn_b5 = g_mice[i].btn_b5;
   }

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
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (winraw_is_pressed(
                        wr, joypad_info, binds[port], port, i))
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
         {
            if (id < RARCH_BIND_LIST_END)
               return winraw_is_pressed(wr, joypad_info, binds[port], port, id);
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(wr->joypad, joypad_info,
                  port, index, id, binds[port]);
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && winraw_keyboard_pressed(wr, id);
      case RETRO_DEVICE_MOUSE:
         return winraw_mouse_state(wr, port, false, id);
      case RARCH_DEVICE_MOUSE_SCREEN:
         return winraw_mouse_state(wr, port, true, id);
      case RETRO_DEVICE_LIGHTGUN:
			switch ( id )
			{
				/*aiming*/
				case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
				case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
				case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
					return winraw_lightgun_aiming_state( wr, port, id );

				/*buttons*/
				case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_TRIGGER);
				case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_RELOAD);
				case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_AUX_A);
				case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_AUX_B);
				case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_AUX_C);
				case RETRO_DEVICE_ID_LIGHTGUN_START:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_START);
				case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_SELECT);
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_UP);
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_DOWN);
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_LEFT);
				case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_RIGHT);

				/*deprecated*/
				case RETRO_DEVICE_ID_LIGHTGUN_X:
				case RETRO_DEVICE_ID_LIGHTGUN_Y:
					return winraw_deprecated_lightgun_state(wr, port, id);
				case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
					return winraw_is_pressed(wr, joypad_info, binds[port], port, RARCH_LIGHTGUN_START);
			}
			break;
   }

   return 0;
}

static void winraw_free(void *d)
{
   winraw_input_t *wr = (winraw_input_t*)d;

   if (wr->joypad)
      wr->joypad->destroy();
   winraw_set_mouse_input(NULL, false);
   winraw_set_keyboard_input(NULL);
   winraw_destroy_window(wr->window);
   free(g_mice);
   free(g_keyboard);
   free(wr->mice);
   free(wr);

   g_mouse_xy_mapping_ready = false;
}

static uint64_t winraw_get_capabilities(void *u)
{
   return (1 << RETRO_DEVICE_KEYBOARD) |
          (1 << RETRO_DEVICE_MOUSE) |
          (1 << RETRO_DEVICE_JOYPAD) |
          (1 << RETRO_DEVICE_ANALOG) |
          (1 << RETRO_DEVICE_LIGHTGUN);
}

static void winraw_grab_mouse(void *d, bool grab)
{
   bool r             = false;
   winraw_input_t *wr = (winraw_input_t*)d;

   if (grab == wr->mouse_grab)
      return;

   r = winraw_set_mouse_input(wr->window, grab);
   if (!r)
      return;

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
   winraw_set_rumble,
   winraw_get_joypad_driver,
   NULL,
   false
};
