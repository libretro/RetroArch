/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include "menu_navigation.h"

/**
 * menu_navigation_clear:
 * @menu                  : menu handle
 * @pending_push          : pending push ?
 *
 * Clears the navigation pointer.
 **/
void menu_navigation_clear(menu_handle_t *menu, bool pending_push)
{
   menu->selection_ptr = 0;

   if (driver.menu_ctx && driver.menu_ctx->navigation_clear)
      driver.menu_ctx->navigation_clear(menu, pending_push);
}

/**
 * menu_navigation_decrement:
 * @menu                  : menu handle
 *
 * Decrement the navigation pointer.
 **/
void menu_navigation_decrement(menu_handle_t *menu)
{
   menu->selection_ptr--;

   if (driver.menu_ctx && driver.menu_ctx->navigation_decrement)
      driver.menu_ctx->navigation_decrement(menu);
}

/**
 * menu_navigation_increment:
 * @menu                  : menu handle
 *
 * Increment the navigation pointer.
 **/
void menu_navigation_increment(menu_handle_t *menu)
{
   menu->selection_ptr++;

   if (driver.menu_ctx && driver.menu_ctx->navigation_increment)
      driver.menu_ctx->navigation_increment(menu);
}

/**
 * menu_navigation_set:      
 * @menu                  : menu handle
 * @idx                   : index to set navigation pointer to.
 * @scroll                : should we scroll when needed?
 *
 * Sets navigation pointer to index @idx.
 **/
void menu_navigation_set(menu_handle_t *menu, size_t idx, bool scroll)
{
   menu->selection_ptr = idx; 

   if (driver.menu_ctx && driver.menu_ctx->navigation_set)
      driver.menu_ctx->navigation_set(menu, scroll);
}

/**
 * menu_navigation_set_last:
 * @menu                  : menu handle
 *
 * Sets navigation pointer to last index.
 **/
void menu_navigation_set_last(menu_handle_t *menu)
{
   menu->selection_ptr = menu_list_get_size(driver.menu->menu_list) - 1;

   if (driver.menu_ctx && driver.menu_ctx->navigation_set_last)
      driver.menu_ctx->navigation_set_last(menu);
}

/**
 * menu_navigation_descend_alphabet:
 * @menu                  : menu handle
 * @ptr_out               : Amount of indices to 'scroll' to get
 *                          to the next entry.
 *
 * Descends alphabet.
 * E.g.:
 * If navigation points to an entry called 'Beta',
 * navigation pointer will be set to an entry called 'Alpha'.
 **/
void menu_navigation_descend_alphabet(menu_handle_t *menu, size_t *ptr_out)
{
   size_t i   = 0;
   size_t ptr = *ptr_out;

   if (!menu->scroll.indices.size)
      return;

   if (ptr == 0)
      return;

   i = menu->scroll.indices.size - 1;

   while (i && menu->scroll.indices.list[i - 1] >= ptr)
      i--;
   *ptr_out = menu->scroll.indices.list[i - 1];

   if (driver.menu_ctx && driver.menu_ctx->navigation_descend_alphabet)
      driver.menu_ctx->navigation_descend_alphabet(menu, ptr_out);
}

/**
 * menu_navigation_ascends_alphabet:
 * @menu                  : menu handle
 * @ptr_out               : Amount of indices to 'scroll' to get
 *                          to the next entry.
 *
 * Ascends alphabet.
 * E.g.:
 * If navigation points to an entry called 'Alpha',
 * navigation pointer will be set to an entry called 'Beta'.
 **/
void menu_navigation_ascend_alphabet(menu_handle_t *menu, size_t *ptr_out)
{
   size_t i   = 0;
   size_t ptr = *ptr_out;

   if (!menu->scroll.indices.size)
      return;

   if (ptr == menu->scroll.indices.list[menu->scroll.indices.size - 1])
      return;

   while (i < menu->scroll.indices.size - 1
         && menu->scroll.indices.list[i + 1] <= ptr)
      i++;
   *ptr_out = menu->scroll.indices.list[i + 1];

   if (driver.menu_ctx && driver.menu_ctx->navigation_descend_alphabet)
      driver.menu_ctx->navigation_descend_alphabet(menu, ptr_out);
}
