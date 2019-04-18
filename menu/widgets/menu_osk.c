/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include "../../input/input_driver.h"
#include "../../configuration.h"

static char *osk_grid[45]        = {NULL};

static int osk_ptr               = 0;
static enum osk_type osk_idx     = OSK_LOWERCASE_LATIN;

#ifdef HAVE_LANGEXTRA
/* This file has a UTF8 BOM, we assume HAVE_LANGEXTRA is only enabled for compilers that can support this. */
#include "menu_osk_utf8_pages.h"
#else
/* Otherwise define some ascii-friendly pages. */
static const char *symbols_page1_grid[] = {
                          "1","2","3","4","5","6","7","8","9","0","Bksp",
                          "!","\"","#","$","%","&","'","*","(",")","Enter",
                          "+",",","-","~","/",":",";","=","<",">","Lower",
                          "?","@","[","\\","]","^","_","|","{","}","Next"};

static const char *uppercase_grid[] = {
                          "1","2","3","4","5","6","7","8","9","0","Bksp",
                          "Q","W","E","R","T","Y","U","I","O","P","Enter",
                          "A","S","D","F","G","H","J","K","L","+","Lower",
                          "Z","X","C","V","B","N","M"," ","_","/","Next"};

static const char *lowercase_grid[] = {
                          "1","2","3","4","5","6","7","8","9","0","Bksp",
                          "q","w","e","r","t","y","u","i","o","p","Enter",
                          "a","s","d","f","g","h","j","k","l","@","Upper",
                          "z","x","c","v","b","n","m"," ","-",".","Next"};
#endif

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
   settings_t *settings = config_get_ptr();
   bool is_rgui;

   if (ptr < 0 || !settings)
      return;

   is_rgui = string_is_equal(settings->arrays.menu_driver, "rgui");

#ifdef HAVE_LANGEXTRA
   if (string_is_equal(osk_grid[ptr],"\xe2\x87\xa6")) /* backspace character */
      input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
   else if (string_is_equal(osk_grid[ptr],"\xe2\x8f\x8e")) /* return character */
      input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
   else
   if (string_is_equal(osk_grid[ptr],"\xe2\x87\xa7")) /* up arrow */
      menu_event_set_osk_idx(OSK_UPPERCASE_LATIN);
   else if (string_is_equal(osk_grid[ptr],"\xe2\x87\xa9")) /* down arrow */
      menu_event_set_osk_idx(OSK_LOWERCASE_LATIN);
   else if (string_is_equal(osk_grid[ptr],"\xe2\x8a\x95")) /* plus sign (next button) */
#else
   if (string_is_equal(osk_grid[ptr], "Bksp"))
      input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
   else if (string_is_equal(osk_grid[ptr], "Enter"))
      input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
   else
   if (string_is_equal(osk_grid[ptr], "Upper"))
      menu_event_set_osk_idx(OSK_UPPERCASE_LATIN);
   else if (string_is_equal(osk_grid[ptr], "Lower"))
      menu_event_set_osk_idx(OSK_LOWERCASE_LATIN);
   else if (string_is_equal(osk_grid[ptr], "Next"))
#endif
      if (menu_event_get_osk_idx() < (is_rgui ? OSK_SYMBOLS_PAGE1 : OSK_TYPE_LAST - 1))
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
#ifdef HAVE_LANGEXTRA
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
#endif
      case OSK_SYMBOLS_PAGE1:
         memcpy(osk_grid, symbols_page1_grid, sizeof(uppercase_grid));
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

char** menu_event_get_osk_grid(void)
{
   return osk_grid;
}
