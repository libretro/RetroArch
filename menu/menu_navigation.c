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

#include <boolean.h>

#include "menu_displaylist.h"
#include "menu_navigation.h"
#include "menu_entries.h"

#include "../configuration.h"

menu_navigation_t *menu_navigation_get_ptr(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   if (!entries)
      return NULL;
   return &entries->navigation;
}
/**
 * menu_navigation_clear:
 * @pending_push          : pending push ?
 *
 * Clears the navigation pointer.
 **/
void menu_navigation_clear(menu_navigation_t *nav, bool pending_push)
{
   if (!nav)
      return;

   nav->selection_ptr = 0;

   menu_driver_navigation_clear(pending_push);
}

/**
 * menu_navigation_decrement:
 *
 * Decrement the navigation pointer.
 **/
void menu_navigation_decrement(menu_navigation_t *nav, unsigned scroll_speed)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   settings_t *settings   = config_get_ptr();

   if (!nav)
      return;

   if (nav->selection_ptr >= scroll_speed)
         menu_navigation_set(nav,
               nav->selection_ptr - scroll_speed, true);
   else
   {
      if (settings->menu.navigation.wraparound.vertical_enable)
         menu_navigation_set(nav, 
               menu_list_get_size(menu_list) - 1, true);
      else
         menu_navigation_set(nav, 0, true);
   }

   menu_driver_navigation_decrement();
}

/**
 * menu_navigation_increment:
 *
 * Increment the navigation pointer.
 **/
void menu_navigation_increment(menu_navigation_t *nav, unsigned scroll_speed)
{
   settings_t *settings   = config_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();

   if (!nav)
      return;

   if (nav->selection_ptr + scroll_speed < (menu_list_get_size(menu_list)))
   {
      menu_navigation_set(nav,
            nav->selection_ptr + scroll_speed, true);
      menu_driver_navigation_increment();
   }
   else
   {
      if (settings->menu.navigation.wraparound.vertical_enable)
         menu_navigation_clear(nav, false);
      else
      {
         if ((menu_list_get_size(menu_list) > 0))
         {
            menu_navigation_set_last(nav);
            menu_driver_navigation_increment();
         }
      }
   }

}

/**
 * menu_navigation_set:      
 * @idx                   : index to set navigation pointer to.
 * @scroll                : should we scroll when needed?
 *
 * Sets navigation pointer to index @idx.
 **/
void menu_navigation_set(menu_navigation_t *nav,
      size_t idx, bool scroll)
{
   if (!nav)
      return;

   nav->selection_ptr = idx; 

   menu_driver_navigation_set(scroll);
}

/**
 * menu_navigation_set_last:
 *
 * Sets navigation pointer to last index.
 **/
void menu_navigation_set_last(menu_navigation_t *nav)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list || !nav)
      return;

   nav->selection_ptr = menu_list_get_size(menu_list) - 1;

   menu_driver_navigation_set_last();
}

/**
 * menu_navigation_descend_alphabet:
 * @ptr_out               : Amount of indices to 'scroll' to get
 *                          to the next entry.
 *
 * Descends alphabet.
 * E.g.:
 * If navigation points to an entry called 'Beta',
 * navigation pointer will be set to an entry called 'Alpha'.
 **/
void menu_navigation_descend_alphabet(menu_navigation_t *nav, size_t *ptr_out)
{
   size_t i   = 0, ptr = *ptr_out;
   if (!nav)
      return;

   if (!nav->scroll.indices.size)
      return;

   if (ptr == 0)
      return;

   i = nav->scroll.indices.size - 1;

   while (i && nav->scroll.indices.list[i - 1] >= ptr)
      i--;
   *ptr_out = nav->scroll.indices.list[i - 1];

   menu_driver_navigation_descend_alphabet(ptr_out);
}

/**
 * menu_navigation_ascends_alphabet:
 * @ptr_out               : Amount of indices to 'scroll' to get
 *                          to the next entry.
 *
 * Ascends alphabet.
 * E.g.:
 * If navigation points to an entry called 'Alpha',
 * navigation pointer will be set to an entry called 'Beta'.
 **/
void menu_navigation_ascend_alphabet(menu_navigation_t *nav, size_t *ptr_out)
{
   size_t i = 0, ptr = *ptr_out;
   if (!nav)
      return;

   if (!nav->scroll.indices.size)
      return;

   if (ptr == nav->scroll.indices.list[nav->scroll.indices.size - 1])
      return;

   while (i < nav->scroll.indices.size - 1
         && nav->scroll.indices.list[i + 1] <= ptr)
      i++;
   *ptr_out = nav->scroll.indices.list[i + 1];

   menu_driver_navigation_ascend_alphabet(ptr_out);
}

ssize_t menu_navigation_get_current_selection(void)
{
   menu_navigation_t *nav = menu_navigation_get_ptr();
   if (!nav)
      return -1;
   return nav->selection_ptr;
}
