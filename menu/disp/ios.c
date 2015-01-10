/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <stdlib.h>
#include <stdio.h>
#include "menu_display.h"
#include "../menu.h"
#include "../../general.h"
#include "ios.h"

static void *ios_init(void)
{
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   if (!menu)
      return NULL;

   menu->userdata = (ios_handle_t*)calloc(1, sizeof(ios_handle_t));
   if (!menu->userdata)
   {
      free(menu);
      return NULL;
   }

   return menu;
}

static void ios_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   if (!menu)
      return;

   if (menu->userdata)
      free(menu->userdata);

   free(menu);
   return;
}

static int menu_ios_iterate(unsigned action)
{
   ios_handle_t *ih = NULL;
   if (!driver.menu)
      return 0;

   ih = (ios_handle_t*)driver.menu->userdata;
   if (ih->switch_to_ios)
      ih->switch_to_ios();

   return 0;
}

static void ios_update_core_info(void *data)
{
   (void)data;
   menu_update_libretro_info(&g_extern.menu.info);
}

menu_ctx_driver_backend_t menu_ctx_backend_ios = {
   menu_ios_iterate,

   "menu_ios",
};

menu_ctx_driver_t menu_ctx_ios = {
  NULL, // set_texture
  NULL, // render_messagebox
  NULL, // render
  NULL, // frame
  ios_init, // init
  NULL, // init_lists
  ios_free, // free
  NULL, // context_reset
  NULL, // context_destroy
  NULL, // populate_entries
  NULL, // iterate
  NULL, // input_postprocess
  NULL, // navigation_clear
  NULL, // navigation_decrement
  NULL, // navigation_increment
  NULL, // navigation_set
  NULL, // navigation_set_last
  NULL, // navigation_descend_alphabet
  NULL, // navigation_ascend_alphabet
  NULL, // list_insert
  NULL, // list_delete
  NULL, // list_clear
  NULL, // list_cache
  NULL, // list_set_selection
  NULL, // init_core_info
  ios_update_core_info,  // ios_update_core_info
  &menu_ctx_backend_ios, // backend
  "ios",
};
