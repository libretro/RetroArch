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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <encodings/utf.h>

#include "menu_osk.h"

#include "../../input/input_keyboard.h"

#if defined(_MSC_VER) && !defined(_XBOX)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif

static const char *osk_grid[45]  = {NULL};

static int osk_ptr               = 0;
static enum osk_type osk_idx     = OSK_LOWERCASE_LATIN;

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

void menu_event_set_osk_idx(enum osk_type idx)
{
   osk_idx = idx;
}

enum osk_type menu_event_get_osk_idx(void)
{
   return osk_idx;
}

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
      menu_event_set_osk_idx(OSK_UPPERCASE_LATIN);
   else if (string_is_equal(osk_grid[ptr],"⇩"))
      menu_event_set_osk_idx(OSK_LOWERCASE_LATIN);
   else if (string_is_equal(osk_grid[ptr],"⊕"))
      if (menu_event_get_osk_idx() < OSK_TYPE_LAST - 1)
         menu_event_set_osk_idx((enum osk_type)(menu_event_get_osk_idx() + 1));
      else
         menu_event_set_osk_idx((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
   else
      input_keyboard_line_append(osk_grid[ptr]);
}

void menu_event_osk_iterate(void)
{
   switch (menu_event_get_osk_idx())
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
}

const char** menu_event_get_osk_grid(void)
{
   return osk_grid;
}
