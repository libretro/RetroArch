/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <ctype.h>

#include <boolean.h>

#include "../configuration.h"
#include "menu_driver.h"
#include "menu_navigation.h"

typedef struct menu_navigation
{
   struct
   {
      /* Quick jumping indices with L/R.
       * Rebuilt when parsing directory. */
      struct
      {
         size_t list[2 * (26 + 2) + 1];
         unsigned size;
      } indices;
      unsigned acceleration;
   } scroll;
   size_t selection_ptr;
} menu_navigation_t;

bool menu_navigation_ctl(enum menu_navigation_ctl_state state, void *data)
{
   static menu_navigation_t menu_navigation_state;
   settings_t          *settings   = config_get_ptr();
   size_t          menu_list_size  = menu_entries_get_size();
   menu_navigation_t        *nav   = &menu_navigation_state;
   size_t selection                = nav->selection_ptr;

   (void)settings;

   switch (state)
   {
      case MENU_NAVIGATION_CTL_DEINIT:
         memset(nav, 0, sizeof(menu_navigation_t));
         break;
      case MENU_NAVIGATION_CTL_CLEAR:
         {
            size_t idx         = 0;
            bool scroll        = true;

            menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
            menu_driver_ctl(RARCH_MENU_CTL_NAVIGATION_CLEAR, data);
         }
         break;
      case MENU_NAVIGATION_CTL_INCREMENT:
         {
            unsigned *scroll_speed = (unsigned*)data;

            if (!scroll_speed)
               return false;

            if (selection >= menu_list_size - 1
                  && !settings->menu.navigation.wraparound.enable)
               return false;

            if ((selection + (*scroll_speed)) < menu_list_size)
            {
               size_t idx  = selection + (*scroll_speed);
               bool scroll = true;
               menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
               menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
               menu_navigation_ctl(MENU_NAVIGATION_CTL_INCREMENT, NULL);
            }
            else
            {
               if (settings->menu.navigation.wraparound.enable)
               {
                  bool pending_push = false;
                  menu_navigation_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
               }
               else
               {
                  if (menu_list_size > 0)
                  {
                     menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_LAST,  NULL);
                     menu_navigation_ctl(MENU_NAVIGATION_CTL_INCREMENT, NULL);
                  }
               }
            }
            
            menu_driver_ctl(RARCH_MENU_CTL_NAVIGATION_INCREMENT, NULL);
         }
         break;
      case MENU_NAVIGATION_CTL_DECREMENT:
         {
            size_t idx             = 0;
            bool scroll            = true;
            unsigned *scroll_speed = (unsigned*)data;

            if (!scroll_speed)
               return false;

            if (selection == 0 
                  && !settings->menu.navigation.wraparound.enable)
               return false;

            if (selection >= *scroll_speed)
               idx = selection - *scroll_speed;
            else
            {
               idx  = menu_list_size - 1;
               if (!settings->menu.navigation.wraparound.enable)
                  idx = 0;
            }

            menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_DECREMENT, NULL);
            menu_driver_ctl(RARCH_MENU_CTL_NAVIGATION_DECREMENT, NULL);

         }
         break;
      case MENU_NAVIGATION_CTL_SET:
         menu_driver_ctl(RARCH_MENU_CTL_NAVIGATION_SET, data);
         break;
      case MENU_NAVIGATION_CTL_SET_LAST:
         {
            size_t new_selection = menu_list_size - 1;
            menu_navigation_ctl(
                  MENU_NAVIGATION_CTL_SET_SELECTION, &new_selection);
            menu_driver_ctl(RARCH_MENU_CTL_NAVIGATION_SET_LAST, NULL);
         }
         break;
      case MENU_NAVIGATION_CTL_ASCEND_ALPHABET:
         {
            size_t i = 0, ptr;
            size_t *ptr_out = nav ? (size_t*)&nav->selection_ptr : NULL;

            if (!nav || !nav->scroll.indices.size || !ptr_out)
               return false;

            ptr = *ptr_out;

            if (ptr == nav->scroll.indices.list[nav->scroll.indices.size - 1])
               return false;

            while (i < nav->scroll.indices.size - 1
                  && nav->scroll.indices.list[i + 1] <= ptr)
               i++;
            *ptr_out = nav->scroll.indices.list[i + 1];

            menu_driver_ctl(RARCH_MENU_CTL_NAVIGATION_ASCEND_ALPHABET, ptr_out);
         }
         break;
      case MENU_NAVIGATION_CTL_DESCEND_ALPHABET:
         {
            size_t i = 0, ptr;
            size_t *ptr_out = nav ? (size_t*)&nav->selection_ptr : NULL;

            if (!nav || !nav->scroll.indices.size || !ptr_out)
               return false;

            ptr = *ptr_out;

            if (ptr == 0)
               return false;

            i   = nav->scroll.indices.size - 1;

            while (i && nav->scroll.indices.list[i - 1] >= ptr)
               i--;
            *ptr_out = nav->scroll.indices.list[i - 1];

            menu_driver_ctl(
                  RARCH_MENU_CTL_NAVIGATION_DESCEND_ALPHABET, ptr_out);
         }
         break;
      case MENU_NAVIGATION_CTL_GET_SELECTION:
         {
            size_t *sel = (size_t*)data;
            if (!nav || !sel)
               return false;
            *sel = selection;
         }
         break;
      case MENU_NAVIGATION_CTL_SET_SELECTION:
         {
            size_t *sel = (size_t*)data;
            if (!nav || !sel)
               return false;
            nav->selection_ptr = *sel;
         }
         break;
      case MENU_NAVIGATION_CTL_CLEAR_SCROLL_INDICES:
         {
            if (!nav)
               return false;
            nav->scroll.indices.size = 0;
         }
         break;
      case MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX:
         {
            size_t *sel = (size_t*)data;
            if (!nav || !sel)
               return false;
            nav->scroll.indices.list[nav->scroll.indices.size++] = *sel;
         }
         break;
      case MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL:
         {
            size_t *sel = (size_t*)data;
            if (!nav || !sel)
               return false;
            *sel = nav->scroll.acceleration;
         }
         break;
      case MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL:
         {
            size_t *sel = (size_t*)data;
            if (!nav || !sel)
               return false;
            nav->scroll.acceleration = *sel;
         }
         break;
      default:
      case MENU_NAVIGATION_CTL_NONE:
         break;
   }

   return true;
}
