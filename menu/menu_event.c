/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2016 - Jean-André Santoni
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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <encodings/utf.h>
#include <string/stdstring.h>

#include "widgets/menu_entry.h"
#include "widgets/menu_input_dialog.h"

#include "menu_event.h"

#include "menu_driver.h"
#include "menu_animation.h"
#include "menu_display.h"
#include "menu_navigation.h"

#include "widgets/menu_dialog.h"

#include "../configuration.h"
#include "../runloop.h"

#if defined(_MSC_VER) && !defined(_XBOX)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif

#define OSK_CHARS_PER_LINE 11

static unsigned char menu_keyboard_key_state[RETROK_LAST];

enum osk_type
{
   OSK_TYPE_UNKNOWN    = 0U,
   OSK_LOWERCASE_LATIN,
   OSK_UPPERCASE_LATIN,
   OSK_HIRAGANA_PAGE1,
   OSK_HIRAGANA_PAGE2,
   OSK_KATAKANA_PAGE1,
   OSK_KATAKANA_PAGE2,
   OSK_TYPE_LAST
};

static int osk_ptr               = 0;
static enum osk_type osk_idx     = OSK_LOWERCASE_LATIN;
static const char *osk_grid[45];

static const char *uppercase_grid[] = {
                          "!","@","#","$","%","^","&","*","(",")","⇦",
                          "Q","W","E","R","T","Y","U","I","O","P","⏎",
                          "A","S","D","F","G","H","J","K","L",":","⇩",
                          "Z","X","C","V","B","N","M"," ","<",">","⊕"};

static const char *lowercase_grid[] = {
                          "1","2","3","4","5","6","7","8","9","0","⇦",
                          "q","w","e","r","t","y","u","i","o","p","⏎",
                          "a","s","d","f","g","h","j","k","l",";","⇧",
                          "z","x","c","v","b","n","m"," ",",",".","⊕"};

static const char *hiragana_page1_grid[] = {
                          "あ","い","う","え","お","ら","り","る","れ","ろ","⇦",
                          "か","き","く","け","こ","が","ぎ","ぐ","げ","ご","⏎",
                          "さ","し","す","せ","そ","ざ","じ","ず","ぜ","ぞ","⇧",
                          "た","ち","つ","て","と","だ","ぢ","づ","で","ど","⊕"};

static const char *hiragana_page2_grid[] = {
                          "な","に","ぬ","ね","の","ば","び","ぶ","べ","ぼ","⇦",
                          "は","ひ","ふ","へ","ほ","ぱ","ぴ","ぷ","ぺ","ぽ","⏎",
                          "ま","み","む","め","も","ん","っ","ゃ","ゅ","ょ","⇧",
                          "や","ゆ","よ","わ","を","ぁ","ぃ","ぅ","ぇ","ぉ","⊕"};

static const char *katakana_page1_grid[] = {
                          "ア","イ","ウ","エ","オ","ラ","リ","ル","レ","ロ","⇦",
                          "カ","キ","ク","ケ","コ","ガ","ギ","グ","ゲ","ゴ","⏎",
                          "サ","シ","ス","セ","ソ","ザ","ジ","ズ","ゼ","ゾ","⇧",
                          "タ","チ","ツ","テ","ト","ダ","ヂ","ヅ","デ","ド","⊕"};

static const char *katakana_page2_grid[] = {
                          "ナ","ニ","ヌ","ネ","ノ","バ","ビ","ブ","ベ","ボ","⇦",
                          "ハ","ヒ","フ","ヘ","ホ","パ","ピ","プ","ペ","ポ","⏎",
                          "マ","ミ","ム","メ","モ","ン","ッ","ャ","ュ","ョ","⇧",
                          "ヤ","ユ","ヨ","ワ","ヲ","ァ","ィ","ゥ","ェ","ォ","⊕"};

int menu_event_get_osk_ptr(void)
{
   return osk_ptr;
}

void menu_event_set_osk_ptr(int i)
{
   osk_ptr = i;
}

void menu_event_osk_append(int ptr)
{
   if (ptr < 0)
      return;

   if (string_is_equal(osk_grid[ptr],"⇦"))
      input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
   else if (string_is_equal(osk_grid[ptr],"⏎"))
      input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
   else if (string_is_equal(osk_grid[ptr],"⇧"))
      osk_idx = OSK_UPPERCASE_LATIN;
   else if (string_is_equal(osk_grid[ptr],"⇩"))
      osk_idx = OSK_LOWERCASE_LATIN;
   else if (string_is_equal(osk_grid[ptr],"⊕"))
      if (osk_idx < OSK_TYPE_LAST - 1)
         osk_idx = (enum osk_type)(osk_idx + 1);
      else
         osk_idx = (enum osk_type)(OSK_TYPE_UNKNOWN + 1);
   else
      input_keyboard_line_append(osk_grid[ptr]);
}

const char** menu_event_get_osk_grid(void)
{
   return osk_grid;
}

static int menu_event_pointer(unsigned *action)
{
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   menu_input_t *menu_input                     = menu_input_get_ptr();
   unsigned fb_width                            = menu_display_get_width();
   unsigned fb_height                           = menu_display_get_height();
   int pointer_device                           =
      menu_driver_ctl(RARCH_MENU_CTL_IS_SET_TEXTURE, NULL) ?
        RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;
   int pointer_x                                =
      current_input->input_state(current_input_data, binds,
            0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_X);
   int pointer_y                                =
      current_input->input_state(current_input_data, binds,
            0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_Y);

   menu_input->pointer.pressed[0]  = current_input->input_state(current_input_data, binds,
         0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.pressed[1]  = current_input->input_state(current_input_data, binds,
         0, pointer_device, 1, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.back        = current_input->input_state(current_input_data, binds,
            0, pointer_device, 0, RARCH_DEVICE_ID_POINTER_BACK);

   menu_input->pointer.x = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
   menu_input->pointer.y = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;

   return 0;
}

unsigned char menu_event_keyboard_is_set(enum retro_key key)
{
   return menu_keyboard_key_state[key];
}

void menu_event_keyboard_set(bool down, enum retro_key key)
{
   if (key == RETROK_UNKNOWN)
   {
      unsigned i;

      for (i = 0; i < RETROK_LAST; i++)
         menu_keyboard_key_state[i] = (menu_keyboard_key_state[i] & 1) << 1;
   }
   else
      menu_keyboard_key_state[key]  = ((menu_keyboard_key_state[key] & 1) << 1) | down;
}

unsigned menu_event(uint64_t input, uint64_t trigger_input)
{
   menu_animation_ctx_delta_t delta;
   float delta_time;
   /* Used for key repeat */
   static float delay_timer                = 0.0f;
   static float delay_count                = 0.0f;
   unsigned ret                            = MENU_ACTION_NOOP;
   static bool initial_held                = true;
   static bool first_held                  = false;
   bool set_scroll                         = false;
   bool mouse_enabled                      = false;
   size_t new_scroll_accel                 = 0;
   menu_input_t *menu_input                = NULL;
   settings_t *settings                    = config_get_ptr();
   static unsigned ok_old                  = 0;
   unsigned menu_ok_btn                    = settings->input.menu_swap_ok_cancel_buttons ?
      RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A;
   unsigned menu_cancel_btn                = settings->input.menu_swap_ok_cancel_buttons ?
      RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B;
   unsigned ok_current                     = input & UINT64_C(1) << menu_ok_btn;
   unsigned ok_trigger                     = ok_current & ~ok_old;

   ok_old                                  = ok_current;

   if (input)
   {
      if (!first_held)
      {
         /* don't run anything first frame, only capture held inputs
          * for old_input_state. */

         first_held  = true;
         delay_timer = initial_held ? 12 : 6;
         delay_count = 0;
      }

      if (delay_count >= delay_timer)
      {
         uint64_t input_repeat = 0;
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_UP);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_DOWN);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_LEFT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_L);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_R);

         set_scroll           = true;
         first_held           = false;
         trigger_input |= input & input_repeat;

         menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL,
               &new_scroll_accel);

         new_scroll_accel = MIN(new_scroll_accel + 1, 64);
      }

      initial_held  = false;
   }
   else
   {
      set_scroll   = true;
      first_held   = false;
      initial_held = true;
   }

   if (set_scroll)
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL,
            &new_scroll_accel);

   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);

   delta.current = delta_time;

   if (menu_animation_ctl(MENU_ANIMATION_CTL_IDEAL_DELTA_TIME_GET, &delta))
      delay_count += delta.ideal;

   if (menu_input_dialog_get_display_kb())
   {
      switch (osk_idx)
      {
         case OSK_HIRAGANA_PAGE1:
            memcpy(osk_grid, hiragana_page1_grid, sizeof(hiragana_page1_grid));
            break;
         case OSK_HIRAGANA_PAGE2:
            memcpy(osk_grid, hiragana_page2_grid, sizeof(hiragana_page2_grid));
            break;
         case OSK_KATAKANA_PAGE1:
            memcpy(osk_grid, katakana_page1_grid, sizeof(katakana_page1_grid));
            break;
         case OSK_KATAKANA_PAGE2:
            memcpy(osk_grid, katakana_page2_grid, sizeof(katakana_page2_grid));
            break;
         case OSK_UPPERCASE_LATIN:
            memcpy(osk_grid, uppercase_grid, sizeof(uppercase_grid));
            break;
         case OSK_LOWERCASE_LATIN:
         default:
            memcpy(osk_grid, lowercase_grid, sizeof(lowercase_grid));
            break;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         if (osk_ptr < 33)
            osk_ptr = osk_ptr + OSK_CHARS_PER_LINE;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
      {
         if (osk_ptr >= OSK_CHARS_PER_LINE)
            osk_ptr = osk_ptr - OSK_CHARS_PER_LINE;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      {
         if (osk_ptr < 43)
            osk_ptr = osk_ptr + 1;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
      {
         if (osk_ptr >= 1)
            osk_ptr = osk_ptr - 1;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L))
      {
         if (osk_idx > OSK_TYPE_UNKNOWN + 1)
            osk_idx = (enum osk_type)(osk_idx - 1);
         else
            osk_idx = (enum osk_type)(OSK_TYPE_LAST - 1);
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R))
      {
         if (osk_idx < OSK_TYPE_LAST - 1)
            osk_idx = (enum osk_type)(osk_idx + 1);
         else
            osk_idx = (enum osk_type)(OSK_TYPE_UNKNOWN + 1);
      }

      if (trigger_input & (UINT64_C(1) << menu_ok_btn))
      {
         if (osk_ptr >= 0)
            menu_event_osk_append(osk_ptr);
      }

      if (trigger_input & (UINT64_C(1) << menu_cancel_btn))
      {
         input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
      }

      /* send return key to close keyboard input window */
      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      trigger_input = 0;
      ok_trigger    = 0;
   }

   if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
      ret = MENU_ACTION_UP;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
      ret = MENU_ACTION_DOWN;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
      ret = MENU_ACTION_LEFT;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      ret = MENU_ACTION_RIGHT;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L))
      ret = MENU_ACTION_SCROLL_UP;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R))
      ret = MENU_ACTION_SCROLL_DOWN;
   else if (ok_trigger)
      ret = MENU_ACTION_OK;
   else if (trigger_input & (UINT64_C(1) << menu_cancel_btn))
      ret = MENU_ACTION_CANCEL;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X))
      ret = MENU_ACTION_SEARCH;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y))
      ret = MENU_ACTION_SCAN;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
      ret = MENU_ACTION_START;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT))
      ret = MENU_ACTION_INFO;
   else if (trigger_input & (UINT64_C(1) << RARCH_MENU_TOGGLE))
      ret = MENU_ACTION_TOGGLE;

   if (menu_keyboard_key_state[RETROK_F11])
   {
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
      menu_keyboard_key_state[RETROK_F11] = false;
   }

   if (runloop_cmd_press(trigger_input, RARCH_QUIT_KEY))
      return MENU_ACTION_QUIT;

   mouse_enabled                      = settings->menu.mouse.enable;
#ifdef HAVE_OVERLAY
   if (!mouse_enabled)
      mouse_enabled = !(settings->input.overlay_enable
            && input_overlay_is_alive(overlay_ptr));
#endif

   if (!(menu_input = menu_input_get_ptr()))
      return 0;

   if (!mouse_enabled)
      menu_input->mouse.ptr = 0;

   if (settings->menu.pointer.enable)
      menu_event_pointer(&ret);
   else
   {
      menu_input->pointer.x          = 0;
      menu_input->pointer.y          = 0;
      menu_input->pointer.dx         = 0;
      menu_input->pointer.dy         = 0;
      menu_input->pointer.accel      = 0;
      menu_input->pointer.pressed[0] = false;
      menu_input->pointer.pressed[1] = false;
      menu_input->pointer.back       = false;
      menu_input->pointer.ptr        = 0;
   }

   return ret;
}
