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

void menu_clear_navigation(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->selection_ptr = 0;

   if (driver.menu_ctx && driver.menu_ctx->navigation_clear)
      driver.menu_ctx->navigation_clear(rgui);
}

void menu_decrement_navigation(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->selection_ptr--;

   if (driver.menu_ctx && driver.menu_ctx->navigation_decrement)
      driver.menu_ctx->navigation_decrement(rgui);
}

void menu_increment_navigation(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->selection_ptr++;

   if (driver.menu_ctx && driver.menu_ctx->navigation_increment)
      driver.menu_ctx->navigation_increment(rgui);
}

void menu_set_navigation(void *data, size_t i)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->selection_ptr = i; 

   if (driver.menu_ctx && driver.menu_ctx->navigation_set)
      driver.menu_ctx->navigation_set(rgui);
}

void menu_set_navigation_last(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->selection_ptr = file_list_get_size(rgui->selection_buf) - 1;

   if (driver.menu_ctx && driver.menu_ctx->navigation_set_last)
      driver.menu_ctx->navigation_set_last(rgui);
}

void menu_descend_alphabet(void *data, size_t *ptr_out)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (!rgui->scroll_indices_size)
      return;
   size_t ptr = *ptr_out;
   if (ptr == 0)
      return;
   size_t i = rgui->scroll_indices_size - 1;
   while (i && rgui->scroll_indices[i - 1] >= ptr)
      i--;
   *ptr_out = rgui->scroll_indices[i - 1];

   if (driver.menu_ctx && driver.menu_ctx->navigation_descend_alphabet)
      driver.menu_ctx->navigation_descend_alphabet(rgui, ptr_out);
}

void menu_ascend_alphabet(void *data, size_t *ptr_out)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (!rgui->scroll_indices_size)
      return;
   size_t ptr = *ptr_out;
   if (ptr == rgui->scroll_indices[rgui->scroll_indices_size - 1])
      return;
   size_t i = 0;
   while (i < rgui->scroll_indices_size - 1 && rgui->scroll_indices[i + 1] <= ptr)
      i++;
   *ptr_out = rgui->scroll_indices[i + 1];

   if (driver.menu_ctx && driver.menu_ctx->navigation_descend_alphabet)
      driver.menu_ctx->navigation_descend_alphabet(rgui, ptr_out);
}
