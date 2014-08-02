/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include <limits.h>
#include <ctype.h>
#include "menu_common.h"
#include "menu_navigation.h"

void menu_clear_navigation(menu_handle_t *menu)
{
   menu->selection_ptr = 0;

   if (driver.menu_ctx && driver.menu_ctx->navigation_clear)
      driver.menu_ctx->navigation_clear(menu);
}

void menu_decrement_navigation(menu_handle_t *menu)
{
   menu->selection_ptr--;

   if (driver.menu_ctx && driver.menu_ctx->navigation_decrement)
      driver.menu_ctx->navigation_decrement(menu);
}

void menu_increment_navigation(menu_handle_t *menu)
{
   menu->selection_ptr++;

   if (driver.menu_ctx && driver.menu_ctx->navigation_increment)
      driver.menu_ctx->navigation_increment(menu);
}

void menu_set_navigation(menu_handle_t *menu, size_t i)
{
   menu->selection_ptr = i; 

   if (driver.menu_ctx && driver.menu_ctx->navigation_set)
      driver.menu_ctx->navigation_set(menu);
}

void menu_set_navigation_last(menu_handle_t *menu)
{
   menu->selection_ptr = file_list_get_size(menu->selection_buf) - 1;

   if (driver.menu_ctx && driver.menu_ctx->navigation_set_last)
      driver.menu_ctx->navigation_set_last(menu);
}

void menu_descend_alphabet(menu_handle_t *menu, size_t *ptr_out)
{
   size_t i, ptr;
   if (!menu->scroll_indices_size)
      return;

   ptr = *ptr_out;

   if (ptr == 0)
      return;

   i = menu->scroll_indices_size - 1;

   while (i && menu->scroll_indices[i - 1] >= ptr)
      i--;
   *ptr_out = menu->scroll_indices[i - 1];

   if (driver.menu_ctx && driver.menu_ctx->navigation_descend_alphabet)
      driver.menu_ctx->navigation_descend_alphabet(menu, ptr_out);
}

void menu_ascend_alphabet(menu_handle_t *menu, size_t *ptr_out)
{
   size_t i   = 0;
   size_t ptr = *ptr_out;

   if (!menu->scroll_indices_size)
      return;

   if (ptr == menu->scroll_indices[menu->scroll_indices_size - 1])
      return;

   while (i < menu->scroll_indices_size - 1 && menu->scroll_indices[i + 1] <= ptr)
      i++;
   *ptr_out = menu->scroll_indices[i + 1];

   if (driver.menu_ctx && driver.menu_ctx->navigation_descend_alphabet)
      driver.menu_ctx->navigation_descend_alphabet(menu, ptr_out);
}
