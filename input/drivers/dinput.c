/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifdef _MSC_VER
#pragma comment(lib, "dinput8")
#endif

#define WIN32_LEAN_AND_MEAN

#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include <dbt.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <boolean.h>
#include <retro_atomic.h>

#include <windowsx.h>

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL                  0x20e
#endif

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef _XBOX
#include "../../gfx/common/win32_common.h"
#endif

#include "../input_keymaps.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

/* Context has to be global as joypads also ride on this context. */
LPDIRECTINPUT8 g_dinput_ctx;

/* Touches, as WM_POINTER* hands them: a fixed set of slots the window
 * procedure writes and the poll reads, with no lock. The window
 * procedure runs on the thread that owns the window - with threaded
 * video, not the poll's - so the list of malloc()ed nodes this was,
 * added to and freed there while the poll walked it, was a race that
 * could read freed memory.
 *
 * One writer, the window procedure: it fills a slot's position and
 * order and then publishes the slot by storing its id (plus one, so 0
 * is a free slot); it frees one by storing 0. The poll reads the id
 * with acquire and then the rest. A touch's position and its id can be
 * a frame apart at worst. Touches are given out in the order they went
 * down, as the list kept them. */
#define DINPUT_MAX_POINTERS 16

struct dinput_pointer_slot
{
   retro_atomic_int_t id_plus1;  /* 0: free */
   retro_atomic_int_t order;     /* when it went down */
   retro_atomic_int_t pos;       /* x << 16 | y, client coordinates */
};


/* What the window procedure hands the poll besides touches, in one word
 * both change with atomic operations: the wheel's notches, a click that
 * focused the window or landed on its title bar, and the left/right
 * Shift and Alt held (for the releases the OS does not send). The rest
 * of the flags are the poll's alone. */
enum dinput_msg_flags
{
   DINP_MSG_SHIFT_L      = (1 << 0),
   DINP_MSG_SHIFT_R      = (1 << 1),
   DINP_MSG_ALT_L        = (1 << 2),
   DINP_MSG_ALT_R        = (1 << 3),
   DINP_MSG_DBCLK_TITLE  = (1 << 4),
   DINP_MSG_WHEEL_UP     = (1 << 5),
   DINP_MSG_WHEEL_DOWN   = (1 << 6),
   DINP_MSG_HWHEEL_UP    = (1 << 7),
   DINP_MSG_HWHEEL_DOWN  = (1 << 8),
   DINP_MSG_MOUSE_IGNORE = (1 << 9)
};

enum dinput_input_flags
{
   /* bits 0-3: the Shift and Alt keys, now in msg_flags */
   DINP_FLAG_DBCLK_ON_TITLEBAR = (1 << 4),
   DINP_FLAG_MOUSE_L_BTN       = (1 << 5),
   DINP_FLAG_MOUSE_R_BTN       = (1 << 6),
   DINP_FLAG_MOUSE_M_BTN       = (1 << 7),
   DINP_FLAG_MOUSE_B4_BTN      = (1 << 8),
   DINP_FLAG_MOUSE_B5_BTN      = (1 << 9),
   DINP_FLAG_MOUSE_WU_BTN      = (1 << 10),
   DINP_FLAG_MOUSE_WD_BTN      = (1 << 11),
   DINP_FLAG_MOUSE_HWU_BTN     = (1 << 12),
   DINP_FLAG_MOUSE_HWD_BTN     = (1 << 13),
   DINP_FLAG_MOUSE_IGNORE      = (1 << 14)
};

struct dinput_input
{
   char *joypad_drv_name;
   LPDIRECTINPUTDEVICE8 keyboard;
   LPDIRECTINPUTDEVICE8 mouse;
   const input_device_driver_t *joypad;
   struct dinput_pointer_slot pointers[DINPUT_MAX_POINTERS];
   retro_atomic_int_t pointer_order;  /* the window procedure's */
   retro_atomic_int_t msg_flags;      /* enum dinput_msg_flags */
   uint32_t mouse_rel;   /* VIDEO_POS_PACK */
   uint32_t mouse_pos;   /* VIDEO_POS_PACK */
   uint16_t flags;
   uint8_t state[256];
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
   if (!g_dinput_ctx)
   {
      /* Who said we shouldn't have same call signature in a COM API? <_< */
#ifdef __cplusplus
      if (!(SUCCEEDED(DirectInput8Create(
                     GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                     IID_IDirectInput8,
                     (void**)&g_dinput_ctx, NULL))))
#else
         if (!(SUCCEEDED(DirectInput8Create(
                        GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                        &IID_IDirectInput8,
                        (void**)&g_dinput_ctx, NULL))))
#endif
            return false;
   }
   return true;
}

static void *dinput_init(const char *joypad_driver)
{
   struct dinput_input *di = NULL;

   if (!dinput_init_context())
      return NULL;

   if (!(di = (struct dinput_input*)calloc(1, sizeof(*di))))
      return NULL;

   if (joypad_driver && *joypad_driver)
      di->joypad_drv_name = strdup(joypad_driver);

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx,
               GUID_SysKeyboard,
               &di->keyboard, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx,
               &GUID_SysKeyboard,
               &di->keyboard, NULL)))
#endif
   {
      di->keyboard = NULL;
   }

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx,
               GUID_SysMouse,
               &di->mouse, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(g_dinput_ctx,
               &GUID_SysMouse,
               &di->mouse, NULL)))
#endif
   {
      di->mouse = NULL;
   }

   if (di->keyboard)
   {
      bool input_nowinkey_enable = input_config_get_nowinkey_enable();
      DWORD flags                = DISCL_NONEXCLUSIVE | DISCL_FOREGROUND;
      if (input_nowinkey_enable)
         flags                  |= DISCL_NOWINKEY;

      IDirectInputDevice8_SetDataFormat(di->keyboard, &c_dfDIKeyboard);
      IDirectInputDevice8_SetCooperativeLevel(di->keyboard,
            (HWND)video_driver_window_get(), flags);
      IDirectInputDevice8_Acquire(di->keyboard);
   }

   if (di->mouse)
   {
      IDirectInputDevice8_SetDataFormat(di->mouse, &c_dfDIMouse2);
      IDirectInputDevice8_SetCooperativeLevel(di->mouse, (HWND)video_driver_window_get(),
            DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
      IDirectInputDevice8_Acquire(di->mouse);
   }

   input_keymaps_init_keyboard_lut(rarch_key_map_dinput);

#ifndef _XBOX
   SetWindowLongPtr(main_window.hwnd, GWLP_USERDATA, (LONG_PTR)di);
#endif

   return di;
}

static void dinput_publish_pointers(struct dinput_input *di);

static void dinput_poll(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;

   if (!di)
      return;

   memset(di->state, 0, sizeof(di->state));

   if (di->keyboard)
   {
      if (FAILED(IDirectInputDevice8_GetDeviceState(
                  di->keyboard, sizeof(di->state), di->state)))
      {
         IDirectInputDevice8_Acquire(di->keyboard);
         /* Clear again: GetDeviceState() does not promise to leave the
          * buffer untouched when it fails, and a partial write would
          * otherwise be read as live key state. dinput_joypad_poll()
          * does the same for its own device state. */
         if (FAILED(IDirectInputDevice8_GetDeviceState(
                     di->keyboard, sizeof(di->state), di->state)))
            memset(di->state, 0, sizeof(di->state));
      }
      else
      {
         /* Ignore 'unknown/undefined' key */
         di->state[RETROK_UNKNOWN] = 0;
      }

      /* If both shift keys are pressed simultaneously, the OS will not issue
       * a WM_KEYUP for the first one. That up event will be issued here. */
      if ((retro_atomic_load_acquire_int(&di->msg_flags) & DINP_MSG_SHIFT_L) && !(di->state[DIK_LSHIFT] & 0x80))
      {
         input_keyboard_event(false, RETROK_LSHIFT, 0,
               win32_get_keyboard_mods(), RETRO_DEVICE_KEYBOARD);
         retro_atomic_fetch_and_int(&di->msg_flags, ~DINP_MSG_SHIFT_L);
      }
      if ((retro_atomic_load_acquire_int(&di->msg_flags) & DINP_MSG_SHIFT_R) && !(di->state[DIK_RSHIFT] & 0x80))
      {
         input_keyboard_event(false, RETROK_RSHIFT, 0,
               win32_get_keyboard_mods(), RETRO_DEVICE_KEYBOARD);
         retro_atomic_fetch_and_int(&di->msg_flags, ~DINP_MSG_SHIFT_R);
      }

      /* When using alt-tab, the alt key won't get a WM_KEYUP message from the
       * OS. Instead we issue it here when ALT isn't pressed down anymore. */
      if ((retro_atomic_load_acquire_int(&di->msg_flags) & DINP_MSG_ALT_L) && !(di->state[DIK_LMENU]  & 0x80))
      {
         input_keyboard_event(false, RETROK_LALT, 0,
               win32_get_keyboard_mods(), RETRO_DEVICE_KEYBOARD);
         retro_atomic_fetch_and_int(&di->msg_flags, ~DINP_MSG_ALT_L);
      }
      if ((retro_atomic_load_acquire_int(&di->msg_flags) & DINP_MSG_ALT_R) && !(di->state[DIK_RMENU]  & 0x80))
      {
         input_keyboard_event(false, RETROK_RALT, 0,
               win32_get_keyboard_mods(), RETRO_DEVICE_KEYBOARD);
         retro_atomic_fetch_and_int(&di->msg_flags, ~DINP_MSG_ALT_R);
      }
   }

   /* What the window procedure has handed over since the last poll:
    * the wheel's notches, a focusing click, a title-bar double
    * click. Taken once, into the poll's own flags. */
   {
      int msg = retro_atomic_fetch_and_int(&di->msg_flags,
            ~(DINP_MSG_WHEEL_UP | DINP_MSG_WHEEL_DOWN
             | DINP_MSG_HWHEEL_UP | DINP_MSG_HWHEEL_DOWN
             | DINP_MSG_MOUSE_IGNORE | DINP_MSG_DBCLK_TITLE));
      /* a notch is this frame's: every reader's until the next poll */
      di->flags &= ~(DINP_FLAG_MOUSE_WU_BTN | DINP_FLAG_MOUSE_WD_BTN
            | DINP_FLAG_MOUSE_HWU_BTN | DINP_FLAG_MOUSE_HWD_BTN);
      if (msg & DINP_MSG_WHEEL_UP)
         di->flags |= DINP_FLAG_MOUSE_WU_BTN;
      if (msg & DINP_MSG_WHEEL_DOWN)
         di->flags |= DINP_FLAG_MOUSE_WD_BTN;
      if (msg & DINP_MSG_HWHEEL_UP)
         di->flags |= DINP_FLAG_MOUSE_HWU_BTN;
      if (msg & DINP_MSG_HWHEEL_DOWN)
         di->flags |= DINP_FLAG_MOUSE_HWD_BTN;
      if (msg & DINP_MSG_MOUSE_IGNORE)
         di->flags |= DINP_FLAG_MOUSE_IGNORE;
      if (msg & DINP_MSG_DBCLK_TITLE)
         di->flags |= DINP_FLAG_DBCLK_ON_TITLEBAR;
   }

   if (di->mouse)
   {
      POINT point;
      DIMOUSESTATE2 mouse_state;
      BYTE *rgb_buttons_ptr     = &mouse_state.rgbButtons[0];
      bool swap_mouse_buttons   = (g_win32_flags & WIN32_CMN_FLAG_SWAP_MOUSE_BTNS) ? true : false;
      bool acquired             = true;

      point.x                   = 0;
      point.y                   = 0;

      mouse_state.lX            = 0;
      mouse_state.lY            = 0;
      mouse_state.lZ            = 0;

      for (
            ; rgb_buttons_ptr < mouse_state.rgbButtons + 8
            ; rgb_buttons_ptr++)
         *rgb_buttons_ptr = 0;

      if (FAILED(IDirectInputDevice8_GetDeviceState(
                  di->mouse, sizeof(mouse_state), &mouse_state)))
      {
         IDirectInputDevice8_Acquire(di->mouse);
         if (FAILED(IDirectInputDevice8_GetDeviceState(
                     di->mouse, sizeof(mouse_state), &mouse_state)))
         {
            mouse_state.lX = 0;
            mouse_state.lY = 0;
            mouse_state.lZ = 0;
            for (
                  ; rgb_buttons_ptr < mouse_state.rgbButtons + 8
                  ; rgb_buttons_ptr++)
               *rgb_buttons_ptr = 0;
            acquired = false;
         }
      }

      di->mouse_rel = VIDEO_POS_PACK(mouse_state.lX, mouse_state.lY);

      if (swap_mouse_buttons)
      {
         if (!mouse_state.rgbButtons[1])
            di->flags &= ~DINP_FLAG_DBCLK_ON_TITLEBAR;

         if (di->flags & DINP_FLAG_DBCLK_ON_TITLEBAR)
            di->flags &= ~DINP_FLAG_MOUSE_R_BTN;
         else
         {
            if (mouse_state.rgbButtons[0])
               di->flags |=  DINP_FLAG_MOUSE_R_BTN;
            else
               di->flags &= ~DINP_FLAG_MOUSE_R_BTN;
         }

         if (mouse_state.rgbButtons[1])
            di->flags    |=  DINP_FLAG_MOUSE_L_BTN;
         else
            di->flags    &= ~DINP_FLAG_MOUSE_L_BTN;
      }
      else
      {
         if (!mouse_state.rgbButtons[0])
            di->flags &= ~DINP_FLAG_DBCLK_ON_TITLEBAR;

         if (di->flags & DINP_FLAG_DBCLK_ON_TITLEBAR)
            di->flags &= ~DINP_FLAG_MOUSE_L_BTN;
         else
         {
            if (mouse_state.rgbButtons[0])
               di->flags |=  DINP_FLAG_MOUSE_L_BTN;
            else
               di->flags &= ~DINP_FLAG_MOUSE_L_BTN;
         }

         if (mouse_state.rgbButtons[1])
            di->flags    |=  DINP_FLAG_MOUSE_R_BTN;
         else
            di->flags    &= ~DINP_FLAG_MOUSE_R_BTN;
      }

      if (mouse_state.rgbButtons[2])
         di->flags    |=  DINP_FLAG_MOUSE_M_BTN;
      else
         di->flags    &= ~DINP_FLAG_MOUSE_M_BTN;

      if (mouse_state.rgbButtons[3])
         di->flags    |=  DINP_FLAG_MOUSE_B4_BTN;
      else
         di->flags    &= ~DINP_FLAG_MOUSE_B4_BTN;

      if (mouse_state.rgbButtons[4])
         di->flags    |=  DINP_FLAG_MOUSE_B5_BTN;
      else
         di->flags    &= ~DINP_FLAG_MOUSE_B5_BTN;

      /* No simple way to get absolute coordinates
       * for RETRO_DEVICE_POINTER. Just use Win32 APIs.
       *
       * Only do so while the DirectInput mouse is acquired. The device
       * is opened with DISCL_FOREGROUND, so acquisition fails whenever
       * the window is not in the foreground (minimized, another window
       * on top, the Qt desktop menu focused). GetCursorPos() does not
       * care about focus, so without this gate the menu kept tracking
       * the desktop cursor through an unfocused window and fired hover
       * sounds while buttons and keyboard were correctly blocked. */
      if (acquired)
      {
         GetCursorPos(&point);
         ScreenToClient((HWND)video_driver_window_get(), &point);
         di->mouse_pos = VIDEO_POS_PACK(point.x, point.y);
      }

      /* Ignore application focusing mouse clicks */
      if (di->flags & DINP_FLAG_MOUSE_IGNORE)
      {
         if (mouse_state.rgbButtons[0] || mouse_state.rgbButtons[1])
            di->flags &= ~(DINP_FLAG_MOUSE_L_BTN | DINP_FLAG_MOUSE_R_BTN);
         else if (!mouse_state.rgbButtons[0] && !mouse_state.rgbButtons[1])
            di->flags &= ~DINP_FLAG_MOUSE_IGNORE;
      }
   }

   dinput_publish_pointers(di);
}

static bool dinput_mouse_button_pressed(
      struct dinput_input *di, unsigned port, unsigned key)
{
   switch (key)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return (di->flags & DINP_FLAG_MOUSE_L_BTN)  ? true : false;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return (di->flags & DINP_FLAG_MOUSE_R_BTN)  ? true : false;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return (di->flags & DINP_FLAG_MOUSE_M_BTN)  ? true : false;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return (di->flags & DINP_FLAG_MOUSE_B4_BTN) ? true : false;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return (di->flags & DINP_FLAG_MOUSE_B5_BTN) ? true : false;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return (di->flags & DINP_FLAG_MOUSE_WU_BTN)  ? true : false;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return (di->flags & DINP_FLAG_MOUSE_WD_BTN)  ? true : false;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         return (di->flags & DINP_FLAG_MOUSE_HWU_BTN) ? true : false;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return (di->flags & DINP_FLAG_MOUSE_HWD_BTN) ? true : false;
   }

   return false;
}

/* The mouse's frame and the touches, in the order they went down,
 * handed to the frontend, which answers for the mouse, the pointer and
 * the lightgun's aim. The mouse is the port's whose Mouse Index is 0;
 * the pointer and the lightgun are every port's, a touch read before
 * the mouse, which stands for one touch: its left button. */
static void dinput_publish_pointers(struct dinput_input *di)
{
   input_pointer_frame_t frame;
   uint32_t touch_pos[DINPUT_MAX_POINTERS];
   int order[DINPUT_MAX_POINTERS];
   unsigned i, n    = 0;
   unsigned buttons = 0;

   if (di->flags & DINP_FLAG_MOUSE_L_BTN)
      buttons |= INPUT_POINTER_LEFT;
   if (di->flags & DINP_FLAG_MOUSE_R_BTN)
      buttons |= INPUT_POINTER_RIGHT;
   if (di->flags & DINP_FLAG_MOUSE_M_BTN)
      buttons |= INPUT_POINTER_MIDDLE;
   if (di->flags & DINP_FLAG_MOUSE_B4_BTN)
      buttons |= INPUT_POINTER_BUTTON_4;
   if (di->flags & DINP_FLAG_MOUSE_B5_BTN)
      buttons |= INPUT_POINTER_BUTTON_5;
   if (di->flags & DINP_FLAG_MOUSE_WU_BTN)
      buttons |= INPUT_POINTER_WHEEL_UP;
   if (di->flags & DINP_FLAG_MOUSE_WD_BTN)
      buttons |= INPUT_POINTER_WHEEL_DOWN;
   if (di->flags & DINP_FLAG_MOUSE_HWU_BTN)
      buttons |= INPUT_POINTER_HWHEEL_UP;
   if (di->flags & DINP_FLAG_MOUSE_HWD_BTN)
      buttons |= INPUT_POINTER_HWHEEL_DOWN;
   frame.pos     = di->mouse_pos;
   frame.rel     = di->mouse_rel;
   frame.buttons = (uint16_t)buttons;

   /* the touches that are down, earliest first: a handful, so an
    * insertion as each is taken is enough */
   for (i = 0; i < DINPUT_MAX_POINTERS; i++)
   {
      unsigned j;
      int o, p;
      if (!retro_atomic_load_acquire_int(&di->pointers[i].id_plus1))
         continue;
      o = retro_atomic_load_relaxed_int(&di->pointers[i].order);
      p = retro_atomic_load_relaxed_int(&di->pointers[i].pos);
      for (j = n; j > 0 && o - order[j - 1] < 0; j--)
      {
         order[j]     = order[j - 1];
         touch_pos[j] = touch_pos[j - 1];
      }
      order[j]     = o;
      touch_pos[j] = (uint32_t)p;
      n++;
   }

   input_driver_publish_pointers(&frame, 1,
           INPUT_POINTERS_BY_MOUSE_INDEX | INPUT_POINTERS_AIM_EVERY_PORT
         | INPUT_POINTERS_GUN_AT_TOUCH);
   input_driver_publish_touches(touch_pos, n, (1u << n) - 1);
}

static int16_t dinput_input_state(
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
   struct dinput_input *di    = (struct dinput_input*)data;

   if (port < MAX_USERS)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            {
               int16_t ret = 0;

               if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
               {
                  unsigned i;

                  if (input_config_get_mouse_index(port) == 0)
                  {
                     for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
                     {
                        if (RETRO_KEYBIND_VALID(&binds[port][i]))
                        {
                           if (dinput_mouse_button_pressed(di, port, binds[port][i].mbutton))
                              ret |= (1 << i);
                        }
                     }
                  }

                  if (!keyboard_mapping_blocked)
                  {
                     for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
                     {
                        if (RETRO_KEYBIND_VALID(&binds[port][i]))
                        {
                           if (     (RETRO_KEYBIND_KEY(&binds[port][i]) && RETRO_KEYBIND_KEY(&binds[port][i]) < RETROK_LAST)
                                 && di->state[rarch_keysym_lut[RETRO_KEYBIND_KEY(&binds[port][i])]] & 0x80)
                              ret |= (1 << i);
                        }
                     }
                  }

                  return ret;
               }

               if (id < RARCH_BIND_LIST_END)
               {
                  if (RETRO_KEYBIND_VALID(&binds[port][id]))
                  {
                     if (     RETRO_KEYBIND_KEY(&binds[port][id]) && RETRO_KEYBIND_KEY(&binds[port][id]) < RETROK_LAST
                           && (di->state[rarch_keysym_lut[RETRO_KEYBIND_KEY(&binds[port][id])]] & 0x80)
                           && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                        )
                        return 1;
                     else if (input_config_get_mouse_index(port) == 0)
                     {
                        if (dinput_mouse_button_pressed(di, port, binds[port][id].mbutton))
                           return 1;
                     }
                  }
               }
            }
            break;
         case RETRO_DEVICE_KEYBOARD:
            return (id && id < RETROK_LAST) && di->state[rarch_keysym_lut[(enum retro_key)id]] & 0x80;
         case RETRO_DEVICE_ANALOG:
            {
               int16_t ret           = 0;
               int id_minus_key      = 0;
               int id_plus_key       = 0;
               unsigned id_minus     = 0;
               unsigned id_plus      = 0;
               bool id_plus_valid    = false;
               bool id_minus_valid   = false;

               input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

               id_minus_valid        = RETRO_KEYBIND_VALID(&binds[port][id_minus]);
               id_plus_valid         = RETRO_KEYBIND_VALID(&binds[port][id_plus]);
               id_minus_key          = RETRO_KEYBIND_KEY(&binds[port][id_minus]);
               id_plus_key           = RETRO_KEYBIND_KEY(&binds[port][id_plus]);

               if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
               {
                  unsigned sym = rarch_keysym_lut[(enum retro_key)id_plus_key];
                  if (di->state[sym] & 0x80)
                     ret = 0x7fff;
               }
               if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
               {
                  unsigned sym = rarch_keysym_lut[(enum retro_key)id_minus_key];
                  if (di->state[sym] & 0x80)
                     ret += -0x7fff;
               }
               return ret;
            }
            break;
         /* The mouse, the pointer and the lightgun's aim are the
          * frontend's to answer: dinput_poll() publishes the mouse and
          * the touches. */
         case RETRO_DEVICE_LIGHTGUN:
            switch (id)
            {
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
               case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
                  {
                     unsigned new_id                = input_driver_lightgun_id_convert(id);
                     const uint64_t bind_joykey     = input_config_binds[port][new_id].joykey;
                     const uint64_t bind_joyaxis    = input_config_binds[port][new_id].joyaxis;
                     const uint64_t autobind_joykey = input_autoconf_binds[port][new_id].joykey;
                     const uint64_t autobind_joyaxis= input_autoconf_binds[port][new_id].joyaxis;
                     uint16_t joyport               = joypad_info->joy_idx;
                     float axis_threshold           = joypad_info->axis_threshold;
                     const uint64_t joykey          = (bind_joykey != NO_BTN)
                        ? bind_joykey  : autobind_joykey;
                     const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
                        ? bind_joyaxis : autobind_joyaxis;

                     if (RETRO_KEYBIND_VALID(&binds[port][new_id]))
                     {
                        if ((uint16_t)joykey != NO_BTN && joypad->button(
                                 joyport, (uint16_t)joykey))
                           return 1;
                        if (joyaxis != AXIS_NONE &&
                              ((float)abs(joypad->axis(joyport, joyaxis))
                               / 0x8000) > axis_threshold)
                           return 1;
                        else if ((RETRO_KEYBIND_KEY(&binds[port][new_id]) && RETRO_KEYBIND_KEY(&binds[port][new_id]) < RETROK_LAST)
                              && !keyboard_mapping_blocked
                              && di->state[rarch_keysym_lut[RETRO_KEYBIND_KEY(&binds[port][new_id])]] & 0x80)
                           return 1;
                        else
                        {
                           if (input_config_get_mouse_index(port) == 0)
                           {
                              if (dinput_mouse_button_pressed(di, port, binds[port][new_id].mbutton))
                                 return 1;
                           }
                        }
                     }
                  }
                  break;
            }
            break;
      }
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

/* The window procedure's: a touch's position in client coordinates. */
static int dinput_pointer_pos(WPARAM lParam)
{
   POINT point;
   point.x            = GET_X_LPARAM(lParam);
   point.y            = GET_Y_LPARAM(lParam);
   ScreenToClient((HWND)video_driver_window_get(), &point);
   return (int)VIDEO_POS_PACK(point.x, point.y);
}

static struct dinput_pointer_slot *dinput_pointer_find(
      struct dinput_input *di, int pointer_id)
{
   unsigned i;
   for (i = 0; i < DINPUT_MAX_POINTERS; i++)
      if (retro_atomic_load_relaxed_int(&di->pointers[i].id_plus1)
            == pointer_id + 1)
         return &di->pointers[i];
   return NULL;
}

static void dinput_pointer_down(struct dinput_input *di, int pointer_id,
      WPARAM lParam)
{
   unsigned i;
   if (dinput_pointer_find(di, pointer_id))
      return;
   for (i = 0; i < DINPUT_MAX_POINTERS; i++)
      if (!retro_atomic_load_relaxed_int(&di->pointers[i].id_plus1))
      {
         retro_atomic_store_relaxed_int(&di->pointers[i].pos,
               dinput_pointer_pos(lParam));
         retro_atomic_store_relaxed_int(&di->pointers[i].order,
               retro_atomic_fetch_add_int(&di->pointer_order, 1));
         retro_atomic_store_release_int(&di->pointers[i].id_plus1,
               pointer_id + 1);
         return;
      }
   /* more touches than slots: the rest are not followed */
}

static void dinput_clear_pointers(struct dinput_input *di)
{
   unsigned i;
   for (i = 0; i < DINPUT_MAX_POINTERS; i++)
      retro_atomic_store_release_int(&di->pointers[i].id_plus1, 0);
}

bool dinput_handle_message(void *data,
      UINT message, WPARAM wParam, LPARAM lParam)
{
   struct dinput_input *di = (struct dinput_input *)data;
   /* WM_POINTERDOWN   : Arrives for each new touch event
    *                    with a new ID - add to list.
    * WM_POINTERUP     : Arrives once the pointer is no
    *                    longer down - remove from list.
    * WM_POINTERUPDATE : arrives for both pressed and
    *                    hovering pointers - ignore hovering
   */

   switch (message)
   {
      case WM_SETFOCUS:
      case WM_KILLFOCUS:
         retro_atomic_fetch_or_int(&di->msg_flags, DINP_MSG_MOUSE_IGNORE);
         break;
      case WM_NCLBUTTONDBLCLK:
         retro_atomic_fetch_or_int(&di->msg_flags, DINP_MSG_DBCLK_TITLE);
         break;
      case WM_POINTERDOWN:
         dinput_pointer_down(di, GET_POINTERID_WPARAM(wParam), lParam);
         return true;
      case WM_POINTERUP:
         {
            struct dinput_pointer_slot *slot = dinput_pointer_find(di,
                  GET_POINTERID_WPARAM(wParam));
            if (slot)
               retro_atomic_store_release_int(&slot->id_plus1, 0);
         }
         return true;
      case WM_POINTERUPDATE:
         {
            struct dinput_pointer_slot *slot = dinput_pointer_find(di,
                  GET_POINTERID_WPARAM(wParam));
            if (slot)
               retro_atomic_store_relaxed_int(&slot->pos,
                     dinput_pointer_pos(lParam));
         }
         return true;
      case WM_DEVICECHANGE:
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
         if (  wParam == DBT_DEVICEARRIVAL  ||
               wParam == DBT_DEVICEREMOVECOMPLETE)
         {
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
            /* TODO/FIXME: Don't destroy everything, let's just
             * handle new devices gracefully. Until then, one reinit
             * per burst: see WIN32_HOTPLUG_TIMER_ID. */
            if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
               win32_hotplug_arm();
         }
#endif
         break;
#ifndef _XBOX
      case WM_TIMER:
         if (wParam != WIN32_HOTPLUG_TIMER_ID)
            break;
         if (win32_hotplug_due())
            joypad_driver_reinit(di, di->joypad_drv_name);
         return true;
#endif
      case WM_MOUSEWHEEL:
         if (((short) HIWORD(wParam))/120 > 0)
            retro_atomic_fetch_or_int(&di->msg_flags, DINP_MSG_WHEEL_UP);
         if (((short) HIWORD(wParam))/120 < 0)
            retro_atomic_fetch_or_int(&di->msg_flags, DINP_MSG_WHEEL_DOWN);
         break;
      case WM_MOUSEHWHEEL:
         if (((short) HIWORD(wParam))/120 > 0)
            retro_atomic_fetch_or_int(&di->msg_flags, DINP_MSG_HWHEEL_UP);
         if (((short) HIWORD(wParam))/120 < 0)
            retro_atomic_fetch_or_int(&di->msg_flags, DINP_MSG_HWHEEL_DOWN);
         break;
      case WM_KEYUP:                /* Key released */
      case WM_SYSKEYUP:             /* Key released */
      case WM_KEYDOWN:              /* Key pressed  */
      case WM_SYSKEYDOWN:           /* Key pressed  */
         {
            unsigned keysym       = (lParam >> 16) & 0xff;
            bool extended         = (lParam >> 24) & 0x1;
            int flag              = 0;

            /* extended keys will map to dinput if the high bit is set */
            if (extended)
               keysym |= 0x80;

            switch (keysym)
            {
               case DIK_LSHIFT: flag = DINP_MSG_SHIFT_L; break;
               case DIK_RSHIFT: flag = DINP_MSG_SHIFT_R; break;
               case DIK_LMENU:  flag = DINP_MSG_ALT_L;   break;
               case DIK_RMENU:  flag = DINP_MSG_ALT_R;   break;
            }

            if (flag)
            {
               if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
                  retro_atomic_fetch_or_int(&di->msg_flags, flag);
               /* the poll may have issued the up already: the bit
                * says, and is cleared in the same step */
               else if (!(retro_atomic_fetch_and_int(&di->msg_flags, ~flag)
                        & flag))
                  return true; /* up already issued, or down never happened */
            }
         }
         break;
   }

   return false;
}

static void dinput_free(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   LPDIRECTINPUT8 hold_ctx = g_dinput_ctx;

   if (!di)
      return;

   /* Prevent a joypad driver to kill our context prematurely. */
   g_dinput_ctx = NULL;

#ifndef _XBOX
   SetWindowLongPtr(main_window.hwnd, GWLP_USERDATA, 0);
#endif

   g_dinput_ctx = hold_ctx;

   /* Clear any leftover pointers. */
   dinput_clear_pointers(di);

   if (di->keyboard)
      IDirectInputDevice8_Release(di->keyboard);

   if (di->mouse)
      IDirectInputDevice8_Release(di->mouse);

   if (di->joypad_drv_name)
      free(di->joypad_drv_name);
   di->joypad_drv_name = NULL;

   free(di);

   dinput_destroy_context();
}

static void dinput_grab_mouse(void *data, bool state)
{
   struct dinput_input *di = (struct dinput_input*)data;
   if (!di->mouse)
      return;

   IDirectInputDevice8_Unacquire(di->mouse);
   IDirectInputDevice8_SetCooperativeLevel(di->mouse,
      (HWND)video_driver_window_get(),
      (DISCL_NONEXCLUSIVE | DISCL_FOREGROUND));
   IDirectInputDevice8_Acquire(di->mouse);

#ifndef _XBOX
   win32_clip_window(state);
#endif
}

static uint64_t dinput_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD)
        | (1 << RETRO_DEVICE_MOUSE)
        | (1 << RETRO_DEVICE_KEYBOARD)
        | (1 << RETRO_DEVICE_LIGHTGUN)
        | (1 << RETRO_DEVICE_POINTER)
        | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_dinput = {
   dinput_init,
   dinput_poll,
   dinput_input_state,
   dinput_free,
   NULL,
   NULL,
   dinput_get_capabilities,
   "dinput",
   dinput_grab_mouse,
   NULL,
   NULL
};
