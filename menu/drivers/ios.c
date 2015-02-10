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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../menu_driver.h"
#include "../menu.h"
#include "../../general.h"
#include "ios.h"
#include "../menu_input.h"

#if 1
static int ios_entry_iterate(unsigned action)
{
   ios_handle_t *ios = NULL;
   if (!driver.menu)
      return 0;

   ios = (ios_handle_t*)driver.menu->userdata;
   if (ios->switch_to_ios)
      ios->switch_to_ios();

   return 0;
}
#else
static int ios_entry_iterate(unsigned action)
{
   const char *label = NULL;
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr);

   menu_list_get_last_stack(driver.menu->menu_list, NULL, &label, NULL);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);
   
   return -1;
}
#endif

static void *ios_init(void)
{
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));
   if (!menu)
      goto error;

   menu->userdata = (ios_handle_t*)calloc(1, sizeof(ios_handle_t));
   if (!menu->userdata)
      goto error;

   return menu;
error:
   if (menu->userdata)
      free(menu->userdata);
   if (menu)
      free(menu);
   return NULL;
}

static void ios_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   if (!menu)
      return;

   if (menu->userdata)
      free(menu->userdata);

   free(menu);
}

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
  NULL, // toggle
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
  ios_entry_iterate,
  "ios",
};
