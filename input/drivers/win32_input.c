/*  RetroArch - A frontend for libretro.
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

/* The keyboard and the mouse from nothing but the window and the
 * oldest calls of the Windows API: for a build that has neither raw
 * input (Windows XP) nor DirectInput 8 (DirectX 8), which is what a
 * Windows 95 or NT 4.0 without DirectX can run.
 *
 * - Keys: the window procedure sees every key go down and come up,
 *   and tells this driver (win32_input_key_message()): a bit a
 *   scancode, the scancodes being what the window's key events are
 *   already translated from. The window may be another thread's - the
 *   video thread's - so the bits are atomic words: set and cleared
 *   there, loaded here, and nothing locked.
 * - Mouse: where the cursor is, in the window, at each poll, and what
 *   its buttons are doing (GetAsyncKeyState()). Motion is how far the
 *   cursor went, so it stops at the screen's edge; there is no wheel.
 *   Handed to the frontend, which answers for the mouse, the pointer
 *   and the lightgun's aim.
 *
 * Only while the window is the foreground one: without it no key is
 * down and no button is held. */

#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../../gfx/video_driver.h"

/* By scancode - the extended keys with 0x80 added, as the window
 * procedure hands them over: down. */
#define WIN32_INPUT_KEY_WORDS 8
static retro_atomic_int_t win32_input_keys[WIN32_INPUT_KEY_WORDS];

typedef struct win32_input
{
   uint32_t mouse_pos;     /* in the window: VIDEO_POS_PACK */
   bool     have_pos;
} win32_input_t;

/* From the window procedure, on the window's thread. */
void win32_input_key_message(unsigned scancode, bool down)
{
   retro_atomic_int_t *word = &win32_input_keys[(scancode & 0xff) >> 5];
   int bit                  = (int)(1u << (scancode & 31));
   if (down)
      retro_atomic_fetch_or_int(word, bit);
   else
      retro_atomic_fetch_and_int(word, ~bit);
}

static void win32_input_keys_clear(void)
{
   unsigned i;
   for (i = 0; i < WIN32_INPUT_KEY_WORDS; i++)
      retro_atomic_store_release_int(&win32_input_keys[i], 0);
}

static void *win32_input_init(const char *joypad_driver)
{
   unsigned i;
   win32_input_t *w32 = (win32_input_t*)calloc(1, sizeof(*w32));

   (void)joypad_driver;
   if (!w32)
      return NULL;
   for (i = 0; i < WIN32_INPUT_KEY_WORDS; i++)
      retro_atomic_int_init(&win32_input_keys[i], 0);
   input_keymaps_init_keyboard_lut(rarch_key_map_win32);
   return w32;
}

static void win32_input_free(void *data)
{
   win32_input_keys_clear();
   free(data);
}

static void win32_input_poll(void *data)
{
   input_pointer_frame_t frame;
   POINT pt;
   win32_input_t *w32 = (win32_input_t*)data;
   HWND hwnd          = (HWND)video_driver_window_get();
   bool focused       = hwnd && GetForegroundWindow() == hwnd;

   frame.pos     = w32->mouse_pos;
   frame.rel     = VIDEO_POS_PACK(0, 0);
   frame.buttons = 0;

   if (!focused)
   {
      /* a key let go while another window had the keyboard never
       * said so here */
      win32_input_keys_clear();
      w32->have_pos = false;
   }
   else if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt))
   {
      /* the left and right buttons as the user has them, not as the
       * mouse does */
      bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
      uint32_t pos = VIDEO_POS_PACK(pt.x, pt.y);

      if (w32->have_pos)
         frame.rel  = VIDEO_POS_PACK(
               pt.x - VIDEO_POS_X(w32->mouse_pos),
               pt.y - VIDEO_POS_Y(w32->mouse_pos));
      w32->mouse_pos = pos;
      w32->have_pos  = true;
      frame.pos      = pos;

      if (GetAsyncKeyState(swapped ? VK_RBUTTON : VK_LBUTTON) & 0x8000)
         frame.buttons |= INPUT_POINTER_LEFT;
      if (GetAsyncKeyState(swapped ? VK_LBUTTON : VK_RBUTTON) & 0x8000)
         frame.buttons |= INPUT_POINTER_RIGHT;
      if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
         frame.buttons |= INPUT_POINTER_MIDDLE;
   }

   /* The mouse is the port's whose Mouse Index is 0; the pointer and
    * the lightgun's aim are every port's. */
   input_driver_publish_pointers(&frame, 1,
           INPUT_POINTERS_BY_MOUSE_INDEX | INPUT_POINTERS_AIM_EVERY_PORT
         | INPUT_POINTERS_GUN_BUTTONS_BOUND);
}

static bool win32_input_key_down(unsigned key)
{
   unsigned sc = (unsigned)rarch_keysym_lut[key] & 0xff;
   /* no scancode: not a key this keyboard has */
   if (!sc)
      return false;
   return ((unsigned)retro_atomic_load_acquire_int(
            &win32_input_keys[sc >> 5]) >> (sc & 31)) & 1;
}

/* Which of @keys are down: bit n of @down for keys[n]. */
static void win32_input_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   unsigned word[WIN32_INPUT_KEY_WORDS];

   (void)data;
   (void)port;
   (void)bind;
   /* the eight words once, however many keys are asked after */
   for (i = 0; i < WIN32_INPUT_KEY_WORDS; i++)
      word[i] = (unsigned)retro_atomic_load_acquire_int(&win32_input_keys[i]);
   for (i = 0; i < count; i++)
   {
      unsigned sc = (unsigned)rarch_keysym_lut[keys[i]] & 0xff;
      if (sc && ((word[sc >> 5] >> (sc & 31)) & 1))
         down[i >> 5] |= (1u << (i & 31));
   }
}

static int16_t win32_input_state(
      void *data,
      const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   (void)data;
   (void)joypad;
   (void)joypad_info;
   (void)binds;
   (void)port;
   (void)idx;
   /* The RetroPad's buttons, the hotkeys, a stick's axes and the
    * lightgun's buttons, where they are bound to keys, are the
    * frontend's to answer: it asks win32_input_keys_down() for the
    * keys once a poll. The mouse and the pointer it has from the
    * frame published in the poll. */
   if (device == RETRO_DEVICE_KEYBOARD && id && id < RETROK_LAST)
      return win32_input_key_down(id) ? 1 : 0;
   return 0;
}

static uint64_t win32_input_get_capabilities(void *data)
{
   (void)data;
   return   (1 << RETRO_DEVICE_JOYPAD)
          | (1 << RETRO_DEVICE_MOUSE)
          | (1 << RETRO_DEVICE_KEYBOARD)
          | (1 << RETRO_DEVICE_LIGHTGUN)
          | (1 << RETRO_DEVICE_POINTER)
          | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_win32 = {
   win32_input_init,
   win32_input_poll,
   win32_input_state,
   win32_input_free,
   NULL,                         /* set_sensor_state */
   NULL,                         /* get_sensor_input */
   win32_input_get_capabilities,
   "win32",
   NULL,                         /* grab_mouse */
   NULL,                         /* grab_stdin */
   NULL,                         /* keypress_vibrate */
   NULL,                         /* survives_video */
   win32_input_keys_down
};
